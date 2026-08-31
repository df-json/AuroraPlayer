#include "Metadata.h"
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>
#include <taglib/tstring.h>
#include <taglib/tpropertymap.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4item.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iostream>
#include <csetjmp>
#include <mutex>

#ifdef _WIN32
#include <windows.h>

// Thread-local variables to ensure multi-threaded background scans don't cross paths
static thread_local jmp_buf g_seh_buf;
static thread_local bool g_in_try_read = false;

// Vectored Exception Handler to catch native crashes (Access Violations, etc.)
static LONG CALLBACK TagLibCrashHandler(PEXCEPTION_POINTERS ExceptionInfo)
{
    if (!g_in_try_read)
        return EXCEPTION_CONTINUE_SEARCH;

    DWORD code = ExceptionInfo->ExceptionRecord->ExceptionCode;

    std::cerr
        << "[Aurora] VEH exception code: 0x"
        << std::hex
        << code
        << std::dec
        << "\n";

    if (code == EXCEPTION_ACCESS_VIOLATION ||
        code == EXCEPTION_ILLEGAL_INSTRUCTION ||
        code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
        code == EXCEPTION_STACK_OVERFLOW)
    {
        std::cerr << "[Aurora] VEH intercepting exception\n";

        longjmp(g_seh_buf, 1);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

// Ensure the VEH is registered exactly once per process lifecycle
static void EnsureVEHRegistered()
{
    static std::once_flag veh_flag;
    std::call_once(veh_flag, []()
                   {
        // '1' means call this handler first
        AddVectoredExceptionHandler(1, TagLibCrashHandler); });
}
#endif

std::string MetadataReader::clean(const std::string &s)
{
    std::string x = s;
    while (!x.empty() && (x.back() == '\0' || std::isspace((unsigned char)x.back())))
        x.pop_back();
    size_t i = 0;
    while (i < x.size() && std::isspace((unsigned char)x[i]))
        ++i;
    return x.substr(i);
}
std::string MetadataReader::fallbackTitle(const std::string &p)
{
    auto x = std::filesystem::path(p).stem().string();
    std::replace(x.begin(), x.end(), '_', ' ');
    return x.empty() ? "Unknown Track" : x;
}
static std::string ts(const TagLib::String &s)
{
    return s.to8Bit(true);
}
static bool writeArt(const std::string &audio, const char *data, size_t size, const std::string &ext, std::string &out)
{
    // A malformed/corrupt tag can report a bogus picture size (garbage read as a huge
    // length). Writing that out would mean a multi-GB allocation/write that looks like
    // a freeze before eventually failing — cap it at something no real embedded cover
    // art would ever need.
    constexpr size_t kMaxArtBytes = 32u * 1024u * 1024u; // 32 MB
    if (!data || !size || size > kMaxArtBytes)
        return false;
    std::filesystem::path dir = std::filesystem::path(audio).parent_path() / ".aurora_art";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec)
        return false;
    auto h = std::hash<std::string>{}(audio);
    auto p = dir / (std::to_string(h) + ext);
    std::ofstream f(p, std::ios::binary);
    if (!f)
        return false;
    f.write(data, (std::streamsize)size);
    if (!f)
        return false;
    out = p.string();
    return true;
}

static void extractArt(
    TagLib::FileRef &ref,
    const std::string &path,
    Song &s)
{
    std::cerr << "[Artwork] START: " << path << "\n";

    try
    {
        // ========================================================
        // IMPORTANT:
        // We DO NOT create another TagLib::FileRef here.
        // The FileRef from MetadataReader::read() is reused.
        // ========================================================

        if (ref.isNull())
        {
            std::cerr << "[Artwork] Existing FileRef is NULL\n";
            return;
        }

        if (!ref.file())
        {
            std::cerr << "[Artwork] Existing FileRef has no file\n";
            return;
        }

        std::cerr << "[Artwork] Existing FileRef is valid\n";

        // ========================================================
        // MP3 / MPEG
        // ========================================================

        if (auto *mpg =
                dynamic_cast<TagLib::MPEG::File *>(ref.file()))
        {
            std::cerr << "[Artwork] MPEG file detected\n";

            std::cerr << "[Artwork] Getting ID3v2 tag...\n";

            TagLib::ID3v2::Tag *tag = mpg->ID3v2Tag();

            std::cerr << "[Artwork] ID3v2Tag() returned\n";

            if (!tag)
            {
                std::cerr << "[Artwork] No ID3v2 tag\n";
                return;
            }

            std::cerr << "[Artwork] Getting frame list...\n";

            auto map = tag->frameListMap();

            std::cerr << "[Artwork] Frame list obtained\n";

            auto it = map.find("APIC");

            if (it == map.end())
            {
                std::cerr << "[Artwork] No APIC artwork\n";
                return;
            }

            std::cerr << "[Artwork] APIC frame found\n";

            if (it->second.isEmpty())
            {
                std::cerr << "[Artwork] APIC frame list empty\n";
                return;
            }

            std::cerr << "[Artwork] Getting first picture frame...\n";

            auto *frame = it->second.front();

            if (!frame)
            {
                std::cerr << "[Artwork] Picture frame is NULL\n";
                return;
            }

            auto *pic =
                dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(frame);

            if (!pic)
            {
                std::cerr
                    << "[Artwork] Frame is not AttachedPictureFrame\n";
                return;
            }

            std::cerr
                << "[Artwork] AttachedPictureFrame valid\n";

            std::string mime = ts(pic->mimeType());

            std::cerr
                << "[Artwork] MIME: "
                << mime
                << "\n";

            std::string ext =
                mime.find("png") != std::string::npos
                    ? ".png"
                    : ".jpg";

            std::cerr << "[Artwork] Getting picture data...\n";

            const TagLib::ByteVector &picture =
                pic->picture();

            std::cerr << "[Artwork] Picture data obtained\n";

            if (picture.isEmpty())
            {
                std::cerr << "[Artwork] Picture is empty\n";
                return;
            }

            constexpr size_t kMaxArtBytes =
                32u * 1024u * 1024u;

            if (picture.size() > kMaxArtBytes)
            {
                std::cerr
                    << "[Artwork] Picture exceeds 32MB limit: "
                    << picture.size()
                    << " bytes\n";

                return;
            }

            std::cerr
                << "[Artwork] Picture size: "
                << picture.size()
                << " bytes\n";

            std::cerr << "[Artwork] Writing artwork...\n";

            bool written = writeArt(
                path,
                picture.data(),
                picture.size(),
                ext,
                s.artworkPath);

            if (written)
            {
                std::cerr
                    << "[Artwork] SUCCESS: "
                    << s.artworkPath
                    << "\n";
            }
            else
            {
                std::cerr
                    << "[Artwork] Failed to write artwork\n";
            }

            return;
        }

        // ========================================================
        // FLAC
        // ========================================================

        if (auto *flac =
                dynamic_cast<TagLib::FLAC::File *>(ref.file()))
        {
            std::cerr << "[Artwork] FLAC file detected\n";

            auto pics = flac->pictureList();

            std::cerr
                << "[Artwork] FLAC picture list obtained\n";

            if (pics.isEmpty())
            {
                std::cerr << "[Artwork] No FLAC artwork\n";
                return;
            }

            auto *pic = pics.front();

            if (!pic)
            {
                std::cerr << "[Artwork] FLAC picture is NULL\n";
                return;
            }

            std::string mime = ts(pic->mimeType());

            std::string ext =
                mime.find("png") != std::string::npos
                    ? ".png"
                    : ".jpg";

            const TagLib::ByteVector &picture =
                pic->data();

            constexpr size_t kMaxArtBytes =
                32u * 1024u * 1024u;

            if (picture.isEmpty() ||
                picture.size() > kMaxArtBytes)
            {
                std::cerr
                    << "[Artwork] Invalid FLAC artwork size\n";
                return;
            }

            writeArt(
                path,
                picture.data(),
                picture.size(),
                ext,
                s.artworkPath);

            return;
        }

        // ========================================================
        // MP4 / M4A
        // ========================================================

        if (auto *mp4 =
                dynamic_cast<TagLib::MP4::File *>(ref.file()))
        {
            std::cerr << "[Artwork] MP4/M4A file detected\n";

            auto *tag = mp4->tag();

            if (!tag)
            {
                std::cerr << "[Artwork] No MP4 tag\n";
                return;
            }

            auto map = tag->itemMap();

            auto it = map.find("covr");

            if (it == map.end())
            {
                std::cerr << "[Artwork] No MP4 cover art\n";
                return;
            }

            auto covers = it->second.toCoverArtList();

            if (covers.isEmpty())
            {
                std::cerr << "[Artwork] MP4 cover list empty\n";
                return;
            }

            auto cover = covers.front();

            std::string ext =
                cover.format() ==
                        TagLib::MP4::CoverArt::JPEG
                    ? ".jpg"
                    : ".png";

            const TagLib::ByteVector &picture =
                cover.data();

            constexpr size_t kMaxArtBytes =
                32u * 1024u * 1024u;

            if (picture.isEmpty() ||
                picture.size() > kMaxArtBytes)
            {
                std::cerr
                    << "[Artwork] Invalid MP4 artwork size\n";
                return;
            }

            writeArt(
                path,
                picture.data(),
                picture.size(),
                ext,
                s.artworkPath);

            return;
        }

        std::cerr
            << "[Artwork] Unsupported artwork format\n";
    }
    catch (const std::exception &e)
    {
        std::cerr
            << "[Artwork] C++ exception: "
            << e.what()
            << "\n";
    }
    catch (...)
    {
        std::cerr
            << "[Artwork] Unknown C++ exception\n";
    }

    std::cerr << "[Artwork] END\n";
}
Song MetadataReader::read(const std::string &filePath)
{
    std::cerr << "[Metadata] ENTER read()\n";

    Song s;

    s.filePath = filePath;
    s.title = fallbackTitle(filePath);
    s.artist = "Unknown Artist";
    s.album = "Unknown Album";
    s.albumArtist = "";
    s.dateAdded = std::time(nullptr);

    std::cerr << "[Metadata] Song initialized\n";

    std::cerr << "[Metadata] Creating TagLib::FileRef...\n";

    TagLib::FileRef ref(filePath.c_str());

    std::cerr << "[Metadata] FileRef created\n";

    if (!ref.isNull() && ref.tag())
    {
        std::cerr << "[Metadata] Tag exists\n";

        auto *t = ref.tag();

        std::cerr << "[Metadata] Reading title...\n";

        if (!t->title().isEmpty())
            s.title = clean(ts(t->title()));

        std::cerr << "[Metadata] Reading artist...\n";

        if (!t->artist().isEmpty())
            s.artist = clean(ts(t->artist()));

        std::cerr << "[Metadata] Reading album...\n";

        if (!t->album().isEmpty())
            s.album = clean(ts(t->album()));

        std::cerr << "[Metadata] Reading genre/year/track...\n";

        s.genre = clean(ts(t->genre()));
        s.year = t->year();
        s.trackNumber = t->track();

        std::cerr << "[Metadata] Reading audio properties...\n";

        auto props = ref.audioProperties();

        if (props)
            s.duration = props->lengthInSeconds();

        std::cerr << "[Metadata] Reading property map...\n";

        TagLib::PropertyMap propsMap = t->properties();

        auto it = propsMap.find("ALBUMARTIST");

        if (it != propsMap.end() && !it->second.isEmpty())
        {
            s.albumArtist = clean(ts(it->second.front()));
        }
        else
        {
            s.albumArtist = s.artist;
        }

        std::cerr << "[Metadata] Basic metadata complete\n";
    }
    else
    {
        std::cerr << "[Metadata] No TagLib tag found\n";
    }

    std::cerr << "[Metadata] About to extract artwork...\n";

    extractArt(ref, filePath, s);

    std::cerr << "[Metadata] Artwork extraction complete\n";

    return s;
}

// read() calls into TagLib, which — like any native parser fed arbitrary/untrusted
// files — can crash outright on a malformed file rather than throwing a catchable
// C++ exception (this is what was killing the whole app mid-scan with no message:
// one bad file took the entire background thread down with it, silently, since a
// crash inside a std::thread has nothing to report to). readOneFile() is the actual
// work; tryRead() below wraps it so a single bad file gets skipped and logged
// instead of ending the scan (and the app) for everyone.
static bool readOneFile(const std::string &path, Song &out)
{
    try
    {
        out = MetadataReader::read(path);
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[Aurora] Skipping file (metadata error): " << path << " — " << e.what() << "\n";
        return false;
    }
    catch (...)
    {
        std::cerr << "[Aurora] Skipping file (unknown metadata error): " << path << "\n";
        return false;
    }
}
bool MetadataReader::tryRead(
    const std::string &filePath,
    Song &out)
{
    std::cerr << "[Metadata] TRYREAD ENTER: "
              << filePath << "\n";
#ifdef _WIN32
    EnsureVEHRegistered();
    g_in_try_read = true;

    if (setjmp(g_seh_buf) == 0)
    {
        std::cerr << "[Metadata] Calling readOneFile()\n";

        bool result = readOneFile(filePath, out);

        g_in_try_read = false;

        std::cerr << "[Metadata] SUCCESS: "
                  << (result ? "true" : "false")
                  << " | " << filePath << "\n";

        return result;
    }
    else
    {
        g_in_try_read = false;

        std::cerr << "[Metadata] VEH CAUGHT CRASH: "
                  << filePath << "\n";

        return false;
    }
#else
    return readOneFile(filePath, out);
#endif
}