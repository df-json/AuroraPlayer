#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct Playlist {
    std::int64_t id = 0;
    std::string name;
    std::string description;
    std::string artwork;
    std::vector<std::int64_t> songs;
    std::int64_t createdAt = 0;
    std::int64_t updatedAt = 0;
};
