#pragma once
#include "Database.h"
class Settings
{
public:
    explicit Settings(Database &db) : db_(db) {}
    int playbackThreshold() const;
    float defaultVolume() const;
    void setPlaybackThreshold(int seconds);
    void setDefaultVolume(float value);

private:
    Database &db_;
};
