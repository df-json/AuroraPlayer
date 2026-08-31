#include "App.h"
#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <SDL.h>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <cctype>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

// Locates a TTF that renders as a clean, smooth modern UI font (ImGui's built-in
// default font is a tiny fixed-size pixel/bitmap font that was never meant to be
// scaled — stretching it via FontGlobalScale is what produces the jagged, "font
// smoothing disabled" look). We can't legally bundle Apple's San Francisco font,
// so preference order is: 1) a TTF the user drops in assets/fonts (e.g. the free,
// SF-metric-compatible "Inter" family), 2) Windows' own modern UI font (Segoe UI
// Variable on Win11, falling back to Segoe UI), 3) ImGui's default bitmap font.
static std::string findUiFont()
{
#ifdef _WIN32
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path bundled = "assets/fonts";
    if (fs::is_directory(bundled, ec))
    {
        for (auto &entry : fs::directory_iterator(bundled, ec))
        {
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c)
                           { return (char)std::tolower(c); });
            if (ext == ".ttf" || ext == ".otf")
                return entry.path().string();
        }
    }
    wchar_t fontsDir[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_FONTS, nullptr, 0, fontsDir) == S_OK)
    {
        int n = WideCharToMultiByte(CP_UTF8, 0, fontsDir, -1, nullptr, 0, nullptr, nullptr);
        std::string dir;
        if (n > 1)
        {
            dir.resize(n - 1);
            WideCharToMultiByte(CP_UTF8, 0, fontsDir, -1, dir.data(), n, nullptr, nullptr);
        }
        for (const char *name : {"seguivar.ttf", "segoeui.ttf", "tahoma.ttf"})
        {
            fs::path candidate = fs::path(dir) / name;
            if (fs::exists(candidate, ec))
                return candidate.string();
        }
    }
#endif
    return {};
}

App::~App()
{
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    if (ImGui::GetCurrentContext())
        ImGui::DestroyContext();
    if (renderer_)
        SDL_DestroyRenderer(renderer_);
    if (window_)
        SDL_DestroyWindow(window_);
    SDL_Quit();
#ifdef _WIN32
    CoUninitialize();
#endif
}
bool App::initialize()
{
#ifdef _WIN32
    // The folder-picker dialog (SHBrowseForFolderW with BIF_NEWDIALOGSTYLE, used when
    // adding a music folder) relies on the shell's newer tree-view/drag-drop machinery,
    // which requires COM to already be initialized on this thread. Without this call the
    // dialog can crash the moment a folder is chosen — this is what was crashing "Add
    // Music Folder". Must happen before any SHBrowseForFolderW call, so as early as possible.
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
    {
        std::cerr << "COM initialization failed\n";
        return false;
    }
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0)
    {
        std::cerr << SDL_GetError() << '\n';
        return false;
    }
    // Linear-filter scaled textures (including the ImGui font atlas) instead of the SDL
    // default nearest-neighbor sampling — nearest-neighbor is what makes text and UI look
    // chunky/aliased, similar to Windows' "smooth edges of screen fonts" being turned off.
    // Must be set before the renderer is created.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    window_ = SDL_CreateWindow("Aurora Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1360, 820, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window_)
    {
        std::cerr << SDL_GetError() << '\n';
        return false;
    }
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_)
    {
        std::cerr << SDL_GetError() << '\n';
        return false;
    }
    SDL_SetWindowData(window_, "renderer", renderer_);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    {
        // Replace the default bitmap font with a real TTF, oversampled for crisp
        // scaling, so text stays smooth across the whole 0.8x-1.4x UI Scale range.
        // Glyph range is expanded past plain Basic Latin/Latin-1 to also cover the
        // Unicode blocks the UI's icon buttons use (playback transport symbols,
        // arrows, dingbat heart, geometric shapes) — without this, those show up
        // as blank/missing-glyph boxes instead of actual icons.
        static const ImWchar ranges[] = {
            0x0020,
            0x00FF, // Basic Latin + Latin-1 Supplement
            0x2190,
            0x21FF, // Arrows (shuffle ⇄, repeat ↻)
            0x2300,
            0x23FF, // Miscellaneous Technical (⏮ ⏸ ⏭)
            0x25A0,
            0x25FF, // Geometric Shapes (▶)
            0x2600,
            0x27BF, // Miscellaneous Symbols & Dingbats (♥ ♡ ☰)
            0,
        };
        ImFontConfig cfg;
        cfg.OversampleH = 3;
        cfg.OversampleV = 3;
        cfg.PixelSnapH = false;
        std::string fontPath = findUiFont();
        if (!fontPath.empty())
            io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 19.0f, &cfg, ranges);
        else
            io.Fonts->AddFontDefault(&cfg);
    }
    ImGui::StyleColorsDark();
    auto &st = ImGui::GetStyle();
    st.WindowRounding = 12;
    st.ChildRounding = 10;
    st.FrameRounding = 7;
    st.PopupRounding = 8;
    st.ItemSpacing = ImVec2(10, 8);
    ImGui_ImplSDL2_InitForSDLRenderer(window_, renderer_);
    ImGui_ImplSDLRenderer2_Init(renderer_);
    std::filesystem::create_directories("data");
    db_ = std::make_unique<Database>("data/music.db");
    if (!db_->open() || !db_->initialize())
        return false;
    settings_ = std::make_unique<Settings>(*db_);
    player_ = std::make_unique<AudioPlayer>();
    if (!player_->initialize())
        return false;
    player_->setVolume(settings_->defaultVolume());
    queue_ = std::make_unique<Queue>();
    library_ = std::make_unique<MusicLibrary>(*db_);
    if (db_->libraryFolders().empty())
    {
        // First run, nothing configured yet: default to a "music" folder next to the
        // .exe (this is a portable, no-installer app, so that's the one location that
        // always makes sense regardless of where the user extracted it — no need to
        // make them browse for a folder before the app is useful at all). Just
        // registers it; an empty folder scans instantly via the existing "Rescan"
        // button whenever they actually drop files in.
        wchar_t exePathW[MAX_PATH];
        DWORD n = GetModuleFileNameW(nullptr, exePathW, MAX_PATH);
        std::filesystem::path exeDir = (n > 0 && n < MAX_PATH) ? std::filesystem::path(exePathW).parent_path() : std::filesystem::current_path();
        std::error_code ec;
        std::filesystem::path defaultMusic = exeDir / "music";
        std::filesystem::create_directories(defaultMusic, ec);
        if (!ec)
            library_->addFolder(defaultMusic.string());
    }
    db_->reconcileMissingFiles();
    ui_ = std::make_unique<UI>(*db_, *library_, *player_, *queue_, *settings_);
    ui_->setWindow(window_);
    return true;
}
int App::run()
{
    bool running = true;
    while (running && !ui_->wantsQuit())
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT)
                running = false;
            if (e.type == SDL_KEYDOWN && !e.key.repeat)
            {
                if (e.key.keysym.sym == SDLK_SPACE)
                {
                    if (player_->isPlaying())
                        player_->pause();
                    else
                        player_->play();
                }
                else if (e.key.keysym.sym == SDLK_LEFT)
                    player_->seek(player_->position() - 5);
                else if (e.key.keysym.sym == SDLK_RIGHT)
                    player_->seek(player_->position() + 5);
                else if (e.key.keysym.sym == SDLK_UP)
                    player_->setVolume(player_->volume() + 0.05f);
                else if (e.key.keysym.sym == SDLK_DOWN)
                    player_->setVolume(player_->volume() - 0.05f);
            }
        }
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ui_->render();
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer_, 12, 13, 16, 255);
        SDL_RenderClear(renderer_);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer_);
        SDL_RenderPresent(renderer_);
    }
    return 0;
}
