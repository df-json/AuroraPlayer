#pragma once
#include "Song.h"
#include <string>
class MetadataReader
{
public:
    static Song read(const std::string &filePath);

private:
    static std::string fallbackTitle(const std::string &filePath);
    static std::string clean(const std::string &s);
};
