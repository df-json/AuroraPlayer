#pragma once
#include <cstdint>
#include <string>

struct Song {
    std::int64_t id = 0;
    std::string title;
    std::string artist = "Unknown Artist";
    std::string album = "Unknown Album";
    std::string albumArtist;
    std::string genre;
    int year = 0;
    int trackNumber = 0;
    double duration = 0.0;
    std::string filePath;
    std::string artworkPath;
    std::int64_t dateAdded = 0;
    std::int64_t playCount = 0;
    bool liked = false;
    bool available = true;
};
