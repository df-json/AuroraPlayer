#pragma once
#include "Song.h"
#include <string>
#include <vector>
struct Album
{
    std::string title = "Unknown Album";
    std::string artist = "Unknown Artist";
    int year = 0;
    std::string artworkPath;
    std::vector<Song> songs;
};
