#include "UI.h"
#include "Search.h"
#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static bool folderDialog(std::string &out)
{
#ifdef _WIN32
    BROWSEINFOW bi{};
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpszTitle = L"Choose your music folder";
    PIDLIST_ABSOLUTE pid = SHBrowseForFolderW(&bi);
    if (!pid)
        return false;
    wchar_t path[MAX_PATH];
    bool ok = SHGetPathFromIDListW(pid, path);
    CoTaskMemFree(pid);
    if (!ok)
        return false;
    int n = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, path, -1, s.data(), n, nullptr, nullptr);
    out = s;
    return true;
#else
    return false;
#endif
}
static std::string timeFmt(double sec)
{
    int s = std::max(0, (int)sec);
    return std::to_string(s / 60) + ":" + (s % 60 < 10 ? "0" : "") + std::to_string(s % 60);
}
UI::UI(Database &d, MusicLibrary &l, AudioPlayer &p, Queue &q, Settings &s) : db_(d), library_(l), player_(p), queue_(q), settings_(s) {}
UI::~UI() { clearTextures(); }
SDL_Texture *UI::textureFor(const std::string &path)
{
    if (path.empty())
        return nullptr;

    auto it = textures_.find(path);
    if (it != textures_.end())
        return it->second;

    std::printf("[Artwork UI] Loading: %s\n", path.c_str());

    if (!std::filesystem::exists(path))
    {
        std::printf("[Artwork UI] FILE DOES NOT EXIST\n");
        return nullptr;
    }

    int w = 0;
    int h = 0;
    int n = 0;

    unsigned char *data = stbi_load(
        path.c_str(),
        &w,
        &h,
        &n,
        4
    );

    if (!data)
    {
        std::printf(
            "[Artwork UI] stbi_load FAILED: %s\n",
            stbi_failure_reason()
        );
        return nullptr;
    }

    std::printf(
        "[Artwork UI] Image loaded: %dx%d channels=%d\n",
        w,
        h,
        n
    );

    SDL_Renderer *r =
        (SDL_Renderer *)SDL_GetWindowData(window_, "renderer");

    if (!r)
    {
        std::printf("[Artwork UI] ERROR: renderer is NULL\n");
        stbi_image_free(data);
        return nullptr;
    }

    SDL_Surface *surf =
        SDL_CreateRGBSurfaceWithFormatFrom(
            data,
            w,
            h,
            32,
            w * 4,
            SDL_PIXELFORMAT_RGBA32
        );

    if (!surf)
    {
        std::printf(
            "[Artwork UI] SDL surface creation FAILED: %s\n",
            SDL_GetError()
        );

        stbi_image_free(data);
        return nullptr;
    }

    SDL_Texture *texture =
        SDL_CreateTextureFromSurface(r, surf);

    if (!texture)
    {
        std::printf(
            "[Artwork UI] SDL texture creation FAILED: %s\n",
            SDL_GetError()
        );

        SDL_FreeSurface(surf);
        stbi_image_free(data);
        return nullptr;
    }

    SDL_SetTextureBlendMode(
        texture,
        SDL_BLENDMODE_BLEND
    );
Uint32 format = 0;
int access = 0;
int texW = 0;
int texH = 0;

if (SDL_QueryTexture(
        texture,
        &format,
        &access,
        &texW,
        &texH) == 0)
{
    std::printf(
        "[Artwork UI] Texture OK: %dx%d format=0x%08X access=%d\n",
        texW,
        texH,
        format,
        access
    );
}
else
{
    std::printf(
        "[Artwork UI] SDL_QueryTexture FAILED: %s\n",
        SDL_GetError()
    );
}
    SDL_FreeSurface(surf);
    stbi_image_free(data);

    textures_[path] = texture;

    std::printf(
        "[Artwork UI] SUCCESS: texture created\n"
    );

    return texture;
}
void UI::clearTextures()
{
    for (auto &x : textures_)
        if (x.second)
            SDL_DestroyTexture(x.second);
    textures_.clear();
}
void UI::playSong(const Song &s)
{
    if (!s.available || !std::filesystem::exists(s.filePath))
        return;
    if (player_.load(s))
    {
        player_.play();
        trackedSongId_ = s.id;
        trackedStart_ = 0;
    }
}
void UI::rebuildShuffle(const std::vector<Song> &base)
{
    playOrder_ = base;
    std::mt19937 rng((unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::shuffle(playOrder_.begin(), playOrder_.end(), rng);
    playOrderIndex_ = 0;
}
void UI::advance(bool userRequested)
{
    auto &v = queue_.items();
    if (!v.empty())
    {
        if (auto *s = queue_.next())
        {
            playSong(*s);
            return;
        }
    }
    std::vector<Song> base = db_.songs();
    base.erase(std::remove_if(base.begin(), base.end(), [](const Song &s)
                              { return !s.available; }),
               base.end());
    if (base.empty())
        return;
    if (repeatMode_ == 2 && !userRequested)
    {
        playSong(player_.currentSong());
        return;
    }
    if (shuffle_)
    {
        if (playOrder_.empty())
            rebuildShuffle(base);
        if (playOrderIndex_ + 1 >= playOrder_.size())
        {
            if (repeatMode_ == 1)
                rebuildShuffle(base);
            else
                return;
        }
        else
            ++playOrderIndex_;
        if (!playOrder_.empty())
            playSong(playOrder_[playOrderIndex_]);
        return;
    }
    auto cur = player_.currentSong();
    auto it = std::find_if(base.begin(), base.end(), [&](const Song &s)
                           { return s.id == cur.id; });
    if (it == base.end())
    {
        playSong(base.front());
        return;
    }
    auto idx = (size_t)std::distance(base.begin(), it);
    if (idx + 1 >= base.size())
    {
        if (repeatMode_ == 1)
            playSong(base.front());
    }
    else
        playSong(base[idx + 1]);
}
void UI::previous()
{
    if (player_.position() > 3)
    {
        player_.seek(0);
        return;
    }
    if (auto *s = queue_.previous())
    {
        playSong(*s);
        return;
    }
    auto base = db_.songs();
    auto cur = player_.currentSong();
    auto it = std::find_if(base.begin(), base.end(), [&](const Song &s)
                           { return s.id == cur.id; });
    if (it != base.end() && it != base.begin())
        playSong(*std::prev(it));
}
void UI::handlePlaybackProgress()
{
    if (trackedSongId_ == 0)
        return;
    double p = player_.position();
    double d = player_.duration();
    if (p >= settings_.playbackThreshold() || (d > 0 && p >= d * 0.5))
    {
        if (trackedStart_ == 0)
        {
            db_.addHistory(trackedSongId_, p);
            db_.incrementPlayCount(trackedSongId_);
            trackedStart_ = 1;
        }
    }
    if (player_.isFinished())
    {
        if (repeatMode_ == 2)
            advance(false);
        else
            advance(false);
        trackedSongId_ = 0;
    }
}
static bool songRow(const Song &s, UI &ui, Database &db, Queue &q)
{
    bool clicked = false;
    ImGui::PushID((int)s.id);
    if (!s.available)
        ImGui::BeginDisabled();
    if (ImGui::Selectable((s.title + "##song").c_str(), false, ImGuiSelectableFlags_AllowDoubleClick))
    {
        ui.playSong(s);
        clicked = true;
    }
    ImGui::SameLine(310);
    ImGui::TextDisabled("%s", s.artist.c_str());
    ImGui::SameLine(550);
    ImGui::TextDisabled("%s", s.album.c_str());
    ImGui::SameLine(770);
    ImGui::TextDisabled("%s", timeFmt(s.duration).c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton(s.liked ? "♥" : "♡"))
        db.setLiked(s.id, !s.liked);
    ImGui::SameLine();
    if (ImGui::SmallButton("+"))
        q.add(s);
    if (ImGui::BeginPopupContextItem("menu"))
    {
        if (ImGui::MenuItem("Play"))
            ui.playSong(s);
        if (ImGui::MenuItem("Play Next"))
            q.addNext(s);
        if (ImGui::MenuItem("Add to Queue"))
            q.add(s);
        if (ImGui::MenuItem(s.liked ? "Unlike" : "Like"))
            db.setLiked(s.id, !s.liked);
        if (ImGui::MenuItem("Edit Metadata"))
            ui.openMetadata(s);
        if (ImGui::MenuItem("Open File Location"))
        {
#ifdef _WIN32
            std::string cmd = "explorer.exe /select,\"" + s.filePath + "\"";
            std::system(cmd.c_str());
#endif
        }
        if (ImGui::BeginMenu("Add to Playlist"))
        {
            for (auto &p : db.playlists())
                if (ImGui::MenuItem(p.name.c_str()))
                    db.addSongToPlaylist(p.id, s.id);
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
    if (!s.available)
        ImGui::EndDisabled();
    ImGui::PopID();
    return clicked;
}
void UI::renderSidebar()
{
    ImGui::BeginChild("sidebar", ImVec2(220, 0), true);
    ImGui::TextColored(ImVec4(.45f, .85f, .65f, 1), "AURORA");
    ImGui::TextDisabled("LOCAL MUSIC PLAYER");
    ImGui::Separator();
    const char *items[] = {"Home", "Search", "Library", "Playlists", "Liked Songs", "Recently Played", "Queue", "Settings"};
    for (int i = 0; i < 8; i++)
        if (ImGui::Selectable(items[i], page_ == i, 0, ImVec2(0, 40)))
            page_ = i;
    ImGui::Dummy(ImVec2(0, 10));
    ImGui::TextDisabled("Offline • C++17");
    ImGui::EndChild();
}
void UI::renderHome()
{
    ImGui::Text("Good morning");
    ImGui::TextDisabled("Your music. Your files. No account required.");
    ImGui::Separator();
    ImGui::Text("Recently Played");
    auto recent = db_.recentSongs(8);
    for (auto &s : recent)
        songRow(s, *this, db_, queue_);
    if (recent.empty())
        ImGui::TextDisabled("Play a song to build your history.");
    ImGui::Spacing();
    ImGui::Text("Recently Added");
    auto all = db_.songs();
    int n = 0;
    for (auto &s : all)
    {
        if (n++ >= 8)
            break;
        songRow(s, *this, db_, queue_);
    }
    if (all.empty())
        ImGui::TextDisabled("Add a music folder from Settings.");
}
void UI::renderSearch()
{
    ImGui::Text("Search your library");
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s", searchQuery_.c_str());
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##search", "Songs, artists, albums...", buf, sizeof(buf)))
        searchQuery_ = buf;
    ImGui::Separator();
    auto results = Search::rank(db_.songs(), searchQuery_);
    for (auto &s : results)
        songRow(s, *this, db_, queue_);
    if (!searchQuery_.empty() && results.empty())
        ImGui::TextDisabled("No matches.");
}
void UI::renderLibrary()
{
    ImGui::Text("Your Library");

    const char *tabs[] = {
        "Songs",
        "Albums",
        "Artists"
    };

    for (int i = 0; i < 3; ++i)
    {
        if (i > 0)
            ImGui::SameLine();

        ImGui::PushID(i);

        if (ImGui::Selectable(
                tabs[i],
                libraryTab_ == i,
                ImGuiSelectableFlags_None,
                ImVec2(100, 30)))
        {
            libraryTab_ = i;
        }

        ImGui::PopID();
    }

    ImGui::Separator();

    // Songs
    if (libraryTab_ == 0)
    {
        auto v = db_.songs();

        const char *sorts[] = {
            "Name",
            "Artist",
            "Album",
            "Date Added",
            "Play Count"
        };

        ImGui::SetNextItemWidth(150);
        ImGui::Combo("Sort", &sortMode_, sorts, 5);

        std::sort(
            v.begin(),
            v.end(),
            [&](const Song &a, const Song &b)
            {
                if (sortMode_ == 1)
                    return a.artist < b.artist;

                if (sortMode_ == 2)
                    return a.album < b.album;

                if (sortMode_ == 3)
                    return a.dateAdded > b.dateAdded;

                if (sortMode_ == 4)
                    return a.playCount > b.playCount;

                return a.title < b.title;
            }
        );

        ImGui::TextDisabled("%zu songs", v.size());

        for (auto &s : v)
            songRow(s, *this, db_, queue_);
    }

    // Albums
    else if (libraryTab_ == 1)
    {
        auto albums = db_.albums();

        for (size_t i = 0; i < albums.size(); ++i)
        {
            auto &a = albums[i];

            ImGui::PushID((int)i);

            if (ImGui::Selectable(
                    (a.title + "##album").c_str(),
                    selectedAlbumTitle_ == a.title &&
                    selectedAlbumArtist_ == a.artist))
            {
                selectedAlbumTitle_ = a.title;
                selectedAlbumArtist_ = a.artist;
            }

            ImGui::SameLine(350);

            ImGui::TextDisabled(
                "%s • %d • %zu tracks",
                a.artist.c_str(),
                a.year,
                a.songs.size()
            );

            ImGui::PopID();
        }

        if (!selectedAlbumTitle_.empty())
        {
            auto album = db_.album(
                selectedAlbumTitle_,
                selectedAlbumArtist_
            );

            if (album)
                renderAlbum(*album);
        }
    }

    // Artists
    else
    {
        auto artists = db_.artists();

        for (size_t i = 0; i < artists.size(); ++i)
        {
            auto &a = artists[i];

            ImGui::PushID((int)i);

            if (ImGui::Selectable(
                    (a.name + "##artist").c_str(),
                    selectedArtistName_ == a.name))
            {
                selectedArtistName_ = a.name;
            }

            ImGui::SameLine(350);

            ImGui::TextDisabled(
                "%zu songs",
                a.songs.size()
            );

            ImGui::PopID();
        }

        if (!selectedArtistName_.empty())
        {
            auto artist = db_.artist(selectedArtistName_);

            if (artist)
                renderArtist(*artist);
        }
    }
}
void UI::renderAlbum(const Album &a)
{
    ImGui::Separator();

    if (auto *t = textureFor(a.artworkPath))
    {
        ImGui::TextDisabled("ARTWORK FOUND");

        ImGui::BeginChild(
            "ArtworkTest",
            ImVec2(150, 150),
            true
        );

        ImGui::Image(
            ImTextureRef((ImTextureID)(intptr_t)t),
            ImVec2(120, 120)
        );

        ImGui::EndChild();
    }
    else
    {
        ImGui::TextDisabled("NO ARTWORK");
    }

    ImGui::Text(
        "Album: %s",
        a.title.c_str()
    );

    ImGui::TextDisabled(
        "%s • %d • %zu tracks",
        a.artist.c_str(),
        a.year,
        a.songs.size()
    );

    if (ImGui::Button("Play Album"))
    {
        for (auto &s : a.songs)
            queue_.add(s);

        if (!a.songs.empty())
            playSong(a.songs.front());
    }

    ImGui::SameLine();

    if (ImGui::Button("Shuffle"))
    {
        rebuildShuffle(a.songs);

        if (!playOrder_.empty())
            playSong(playOrder_[0]);
    }

    for (auto &s : a.songs)
        songRow(s, *this, db_, queue_);
}
void UI::renderArtist(const Artist &a)
{
    ImGui::Separator();
if (auto *t = textureFor(a.artworkPath))
{
    ImGui::TextDisabled("ARTWORK FOUND");
    ImGui::Image(
        (ImTextureID)t,
        ImVec2(110, 110)
    );
    ImGui::SameLine();
}
else
{
    ImGui::TextDisabled("NO ARTWORK");
}
    ImGui::Text("Artist: %s", a.name.c_str());
    if (ImGui::Button("Play All"))
    {
        for (auto &s : a.songs)
            queue_.add(s);
        if (!a.songs.empty())
            playSong(a.songs.front());
    }
    ImGui::SameLine();
    if (ImGui::Button("Shuffle"))
    {
        rebuildShuffle(a.songs);
        if (!playOrder_.empty())
            playSong(playOrder_[0]);
    }
    for (auto &s : a.songs)
        songRow(s, *this, db_, queue_);
}
void UI::renderPlaylists()
{
    ImGui::Text("Playlists");
    static char name[128] = "";
    ImGui::SetNextItemWidth(280);
    ImGui::InputText("##newplaylist", name, sizeof(name));
    ImGui::SameLine();
    if (ImGui::Button("Create") && name[0])
    {
        std::int64_t id;
        if (db_.createPlaylist(name, id))
            selectedPlaylist_ = id;
        name[0] = 0;
    }
    ImGui::Separator();
    for (auto &p : db_.playlists())
    {
        ImGui::PushID((int)p.id);
        if (ImGui::Selectable(p.name.c_str(), selectedPlaylist_ == p.id))
            selectedPlaylist_ = p.id;
        ImGui::SameLine();
        if (ImGui::SmallButton("Delete"))
        {
            db_.deletePlaylist(p.id);
            if (selectedPlaylist_ == p.id)
                selectedPlaylist_ = 0;
        }
        ImGui::PopID();
    }
    if (selectedPlaylist_)
    {
        auto ps = db_.playlistSongs(selectedPlaylist_);
        ImGui::Separator();
        if (ImGui::Button("Play Playlist") && !ps.empty())
        {
            queue_.setItems(ps);
            playSong(ps.front());
        }
        ImGui::SameLine();
        if (ImGui::Button("Shuffle Playlist") && !ps.empty())
        {
            rebuildShuffle(ps);
            playSong(playOrder_[0]);
        }
        for (size_t i = 0; i < ps.size(); ++i)
        {
            songRow(ps[i], *this, db_, queue_);
            ImGui::SameLine();
            if (i > 0 && ImGui::SmallButton(("Up##" + std::to_string(ps[i].id)).c_str()))
                db_.reorderPlaylist(selectedPlaylist_, ps[i].id, (std::int64_t)i - 1);
            ImGui::SameLine();
            if (i + 1 < ps.size() && ImGui::SmallButton(("Down##" + std::to_string(ps[i].id)).c_str()))
                db_.reorderPlaylist(selectedPlaylist_, ps[i].id, (std::int64_t)i + 1);
        }
    }
}
void UI::renderLiked()
{
    ImGui::Text("Liked Songs");
    for (auto &s : db_.likedSongs())
        songRow(s, *this, db_, queue_);
}
void UI::renderRecent()
{
    ImGui::Text("Recently Played");
    for (auto &s : db_.recentSongs(100))
        songRow(s, *this, db_, queue_);
}
void UI::renderQueue()
{
    ImGui::Text("Queue");
    if (ImGui::Button("Clear Queue"))
        queue_.clear();
    ImGui::Separator();
    auto v = queue_.items();
    for (size_t i = 0; i < v.size(); ++i)
    {
        ImGui::PushID((int)i);
        ImGui::Text("%zu. %s — %s", i + 1, v[i].title.c_str(), v[i].artist.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Play"))
        {
            queue_.setIndex(i);
            playSong(v[i]);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove"))
            queue_.remove(i);
        ImGui::SameLine();
        if (i > 0 && ImGui::SmallButton("Up"))
            queue_.move(i, i - 1);
        ImGui::SameLine();
        if (i + 1 < v.size() && ImGui::SmallButton("Down"))
            queue_.move(i, i + 1);
        ImGui::PopID();
    }
}
void UI::renderSettings()
{
    ImGui::Text("Settings");
    ImGui::Text("Music Library");
    for (auto &f : db_.libraryFolders())
    {
        ImGui::BulletText("%s", f.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton(("Remove##" + f).c_str()))
            library_.removeFolder(f);
    }
    if (ImGui::Button("Add Music Folder"))
    {
        if (folderDialog(selectedFolder_))
        {
            library_.addFolder(selectedFolder_);
            scanDone_ = 0;
            scanTotal_ = 0;
            library_.scanAsync([this](int done, int total, const std::string &)
                               {scanDone_=done;scanTotal_=total; });
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Rescan"))
    {
        scanDone_ = 0;
        scanTotal_ = 0;
        library_.scanAsync([this](int done, int total, const std::string &)
                           {scanDone_=done;scanTotal_=total; });
    }
    ImGui::TextDisabled("Scanning: %s", library_.scanning() ? "yes" : "no");
    if (scanTotal_ > 0)
    {
        float p = (float)scanDone_ / (float)scanTotal_;
        ImGui::ProgressBar(p, ImVec2(-1, 20));
        ImGui::TextDisabled("%d / %d files", scanDone_.load(), scanTotal_.load());
    }
    ImGui::Separator();
    ImGui::Text("Playback");
    int threshold = settings_.playbackThreshold();
    if (ImGui::SliderInt("History threshold (seconds)", &threshold, 0, 120))
        settings_.setPlaybackThreshold(threshold);
    float vol = settings_.defaultVolume();
    if (ImGui::SliderFloat("Default volume", &vol, 0, 1))
        settings_.setDefaultVolume(vol);
    ImGui::Separator();
    static char backup[512] = "data/music-backup.db";
    ImGui::InputText("Backup file", backup, sizeof(backup));
    if (ImGui::Button("Backup Database"))
        db_.backup(backup);
    ImGui::SameLine();
    if (ImGui::Button("Restore Database"))
    {
        db_.restore(backup);
        db_.reconcileMissingFiles();
    }
    if (ImGui::Button("Reconcile Missing Files"))
        db_.reconcileMissingFiles();
    ImGui::Separator();
    ImGui::Text("Appearance");
    const char *themes[] = {"Dark", "Light"};
    if (ImGui::Combo("Theme", &themeMode_, themes, 2))
    {
        if (themeMode_ == 0)
            ImGui::StyleColorsDark();
        else
            ImGui::StyleColorsLight();
    }
    if (ImGui::SliderFloat("UI Scale", &uiScale_, 0.8f, 1.4f, "%.2fx"))
    {
        ImGui::GetIO().FontGlobalScale = uiScale_;
    }
    ImGui::Separator();
    ImGui::Text("Application");
    ImGui::TextDisabled("Aurora Player 2.0 • Native C++ • Offline");
}
void UI::openMetadata(const Song &s)
{
    editingSong_ = s;
    showMetadata_ = true;
}
void UI::renderMetadataDialog()
{
    if (!showMetadata_)
        return;
    ImGui::OpenPopup("Edit Metadata");
    if (ImGui::BeginPopupModal("Edit Metadata", &showMetadata_, ImGuiWindowFlags_AlwaysAutoResize))
    {
        char title[256], artist[256], album[256], aa[256], genre[256];
        std::snprintf(title, sizeof(title), "%s", editingSong_.title.c_str());
        std::snprintf(artist, sizeof(artist), "%s", editingSong_.artist.c_str());
        std::snprintf(album, sizeof(album), "%s", editingSong_.album.c_str());
        std::snprintf(aa, sizeof(aa), "%s", editingSong_.albumArtist.c_str());
        std::snprintf(genre, sizeof(genre), "%s", editingSong_.genre.c_str());
        if (ImGui::InputText("Title", title, sizeof(title)))
            editingSong_.title = title;
        if (ImGui::InputText("Artist", artist, sizeof(artist)))
            editingSong_.artist = artist;
        if (ImGui::InputText("Album", album, sizeof(album)))
            editingSong_.album = album;
        if (ImGui::InputText("Album Artist", aa, sizeof(aa)))
            editingSong_.albumArtist = aa;
        if (ImGui::InputText("Genre", genre, sizeof(genre)))
            editingSong_.genre = genre;
        ImGui::InputInt("Year", &editingSong_.year);
        ImGui::InputInt("Track Number", &editingSong_.trackNumber);
        ImGui::TextDisabled("Changes are stored in Aurora's database and do not modify the audio file.");
        if (ImGui::Button("Save"))
        {
            db_.updateSongMetadata(editingSong_);
            showMetadata_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            showMetadata_ = false;
        ImGui::EndPopup();
    }
}

void UI::renderPlayerBar()
{
    ImGui::Separator();

    const Song &s = player_.currentSong();

    if (s.filePath.empty())
    {
        ImGui::TextDisabled("Nothing playing");
        return;
    }

    // ------------------------------------------------------------
    // PLAYER BAR
    // ------------------------------------------------------------

    // ------------------------------------------------------------
// Album artwork
// ------------------------------------------------------------

if (auto *t = textureFor(s.artworkPath))
{
    ImGui::Image(
        ImTextureRef((ImTextureID)(intptr_t)t),
        ImVec2(52, 52)
    );

    ImGui::SameLine();
}

// ------------------------------------------------------------
// Fixed-width song information
// ------------------------------------------------------------

ImGui::BeginChild(
    "PlayerSongInfo",
    ImVec2(250, 52),
    false,
    ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoScrollWithMouse
);

ImGui::Text(
    "%s",
    s.title.c_str()
);

ImGui::Text(
    "%s",
    s.artist.c_str()
);

ImGui::EndChild();

ImGui::SameLine();
    // ------------------------------------------------------------
    // Playback controls
    // ------------------------------------------------------------

    if (ImGui::Button("<<", ImVec2(42, 30)))
        previous();

    ImGui::SameLine();

    if (ImGui::Button(player_.isPlaying() ? "||" : ">", ImVec2(42, 30)))
    {
        if (player_.isPlaying())
            player_.pause();
        else
            player_.play();
    }

    ImGui::SameLine();

    if (ImGui::Button(">>", ImVec2(42, 30)))
        advance(true);

    ImGui::SameLine();

    // Shuffle
    if (ImGui::Button(shuffle_ ? "[S]" : "S", ImVec2(42, 30)))
    {
        shuffle_ = !shuffle_;

        if (shuffle_)
            rebuildShuffle(db_.songs());
    }

    ImGui::SameLine();

    // Repeat
    const char *repeatIcon[] =
    {
        "R",
        "RA",
        "R1"
    };

    if (ImGui::Button(repeatIcon[repeatMode_], ImVec2(42, 30)))
    {
        repeatMode_ = (repeatMode_ + 1) % 3;
    }
    ImGui::SameLine();
    if (ImGui::Button("Queue", ImVec2(65, 30)))
        {
        showQueue_ = !showQueue_;
    }

    // ------------------------------------------------------------
    // Progress
    // ------------------------------------------------------------

    float pos = (float)player_.position();
    float dur = (float)player_.duration();

    ImGui::SetNextItemWidth(120);

    if (dur > 0.0f)
    {
        if (ImGui::SliderFloat(
                "##progress",
                &pos,
                0.0f,
                dur,
                ""))
        {
            player_.seek(pos);
        }
    }

    ImGui::SameLine();

    ImGui::Text(
        "%s / %s",
        timeFmt(pos).c_str(),
        timeFmt(dur).c_str()
    );

    ImGui::SameLine();

    // ------------------------------------------------------------
    // Volume
    // ------------------------------------------------------------

    static float uiVolume = 1.0f;

ImGui::SetNextItemWidth(120);

if (ImGui::SliderFloat(
        "##volume",
        &uiVolume,
        0.0f,
        1.0f,
        ""))
{
    player_.setVolume(uiVolume);
}

ImGui::SameLine();

ImGui::Text("%d%%", (int)(uiVolume * 100.0f));

    // ------------------------------------------------------------
    // Queue
    // ------------------------------------------------------------

    if (ImGui::Button("Queue"))
    {
        showQueue_ = !showQueue_;
    }
}
void UI::render()
{
    handlePlaybackProgress();

    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::Begin("Aurora", nullptr, flags);

    // =========================================================
    // MAIN AREA
    // =========================================================

    float playerBarHeight = 72.0f;

    ImGui::BeginChild(
        "MainArea",
        ImVec2(0, -playerBarHeight),
        false,
        ImGuiWindowFlags_NoScrollbar
    );

    // Sidebar
    ImGui::BeginChild(
        "sidebar",
        ImVec2(220, 0),
        true
    );

    ImGui::TextColored(
        ImVec4(.45f, .85f, .65f, 1),
        "AURORA"
    );

    ImGui::TextDisabled("LOCAL MUSIC PLAYER");
    ImGui::Separator();

    const char* items[] =
    {
        "Home",
        "Search",
        "Library",
        "Playlists",
        "Liked Songs",
        "Recently Played",
        "Queue",
        "Settings"
    };

    for (int i = 0; i < 8; i++)
    {
        if (ImGui::Selectable(
                items[i],
                page_ == i,
                0,
                ImVec2(0, 40)))
        {
            page_ = i;
        }
    }

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::TextDisabled("Offline • C++17");

    ImGui::EndChild();

    ImGui::SameLine();

    // Content
    ImGui::BeginChild(
    "content",
    ImVec2(0, 0),
    true,
    ImGuiWindowFlags_None
);

    switch (page_)
    {
    case 0:
        renderHome();
        break;

    case 1:
        renderSearch();
        break;

    case 2:
        renderLibrary();
        break;

    case 3:
        renderPlaylists();
        break;

    case 4:
        renderLiked();
        break;

    case 5:
        renderRecent();
        break;

    case 6:
        renderQueue();
        break;

    case 7:
        renderSettings();
        break;
    }

    ImGui::EndChild();

    ImGui::EndChild();

    // =========================================================
    // FIXED PLAYER BAR
    // =========================================================

    renderPlayerBar();

    ImGui::End();

    // =========================================================
    // QUEUE WINDOW
    // =========================================================

    if (showQueue_)
    {
        ImGui::SetNextWindowSize(
            ImVec2(420, 500),
            ImGuiCond_FirstUseEver
        );

        ImGui::Begin("Queue", &showQueue_);

        renderQueue();

        ImGui::End();
    }

    // =========================================================
    // METADATA
    // =========================================================

    renderMetadataDialog();
}