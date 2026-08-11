#include "Metadata.h"
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>
#include <taglib/tstring.h>
#include <taglib/tpropertymap.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/id3v2attachedpictureframe.h>
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
static std::string ts(const TagLib::String &s) { return s.isNull() ? "" : s.to8Bit(true); }
static int yearFrom(const std::string &s)
{
    try
    {
        return s.empty() ? 0 : std::stoi(s.substr(0, 4));
    }
    catch (...)
    {
        return 0;
    }
}
static bool writeArt(const std::string &audio, const char *data, size_t size, const std::string &ext, std::string &out)
{
    if (!data || !size)
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
static void extractArt(const std::string &path, Song &s)
{
    TagLib::FileRef ref(path.c_str());
    if (ref.isNull() || !ref.file())
        return;
    if (auto *mpg = dynamic_cast<TagLib::MPEG::File *>(ref.file()))
    {
        if (auto *tag = mpg->ID3v2Tag())
        {
            auto map = tag->frameListMap();
            auto it = map.find("APIC");
            if (it != map.end() && !it->second.isEmpty())
            {
                auto *pic = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(it->second.front());
                if (pic)
                {
                    std::string mime = ts(pic->mimeType());
                    std::string ext = mime.find("png") != std::string::npos ? ".png" : ".jpg";
                    writeArt(path, pic->picture().data(), pic->picture().size(), ext, s.artworkPath);
                }
            }
        }
    }
    else if (auto *flac = dynamic_cast<TagLib::FLAC::File *>(ref.file()))
    {
        auto pics = flac->pictureList();
        if (!pics.isEmpty())
        {
            auto *pic = pics.front();
            std::string mime = ts(pic->mimeType());
            std::string ext = mime.find("png") != std::string::npos ? ".png" : ".jpg";
            writeArt(path, pic->data().data(), pic->data().size(), ext, s.artworkPath);
        }
    }
    else if (auto *mp4 = dynamic_cast<TagLib::MP4::File *>(ref.file()))
    {
        if (auto *tag = mp4->tag())
        {
            auto map = tag->itemListMap();
            auto it = map.find("covr");
            if (it != map.end())
            {
                auto covers = it->second.toCoverArtList();
                if (!covers.isEmpty())
                {
                    auto c = covers.front();
                    std::string ext = c.format() == TagLib::MP4::CoverArt::JPEG ? ".jpg" : ".png";
                    writeArt(path, c.data().data(), c.data().size(), ext, s.artworkPath);
                }
            }
        }
    }
}
Song MetadataReader::read(const std::string &filePath)
{
    Song s;
    s.filePath = filePath;
    s.title = fallbackTitle(filePath);
    s.artist = "Unknown Artist";
    s.album = "Unknown Album";
    s.albumArtist = "";
    s.dateAdded = std::time(nullptr);
    TagLib::FileRef ref(filePath.c_str());
    if (!ref.isNull() && ref.tag())
    {
        auto *t = ref.tag();
        if (!t->title().isEmpty())
            s.title = clean(ts(t->title()));
        if (!t->artist().isEmpty())
            s.artist = clean(ts(t->artist()));
        if (!t->album().isEmpty())
            s.album = clean(ts(t->album()));
        s.genre = clean(ts(t->genre()));
        s.year = t->year();
        s.trackNumber = t->track();
        auto props = ref.audioProperties();
        if (props)
            s.duration = props->lengthInSeconds();
        TagLib::PropertyMap propsMap = t->properties();
        auto it = propsMap.find("ALBUMARTIST");
        if (it != propsMap.end() && !it->second.isEmpty())
            s.albumArtist = clean(ts(it->second.front()));
        else
            s.albumArtist = s.artist;
    }
    extractArt(filePath, s);
    return s;
}
