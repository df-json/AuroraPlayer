#pragma once
#include "Song.h"
#include <string>
#include <vector>
class Search { public: static std::vector<Song> rank(const std::vector<Song>& source,const std::string& query); };
