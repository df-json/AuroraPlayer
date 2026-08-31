#pragma once
#include "Song.h"
#include <string>
#include <vector>
struct Artist { std::string name="Unknown Artist"; std::string artworkPath; std::vector<Song> songs; };
