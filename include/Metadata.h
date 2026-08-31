#pragma once
#include "Song.h"
#include <string>
class MetadataReader {
public:
    static Song read(const std::string& filePath);
    // Reads tags/art for one file the same as read(), but isolated so that a single
    // corrupt or malformed file (crash, hang, runaway allocation) can't take down the
    // whole background scan — returns false and logs a warning instead of crashing.
    // Prefer this over read() when scanning a folder of files you don't control.
    static bool tryRead(const std::string& filePath, Song& out);
private: static std::string fallbackTitle(const std::string& filePath); static std::string clean(const std::string& s);
};
