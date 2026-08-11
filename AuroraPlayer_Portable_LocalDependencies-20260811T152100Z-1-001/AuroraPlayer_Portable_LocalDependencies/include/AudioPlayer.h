#pragma once
#include "Song.h"
#include <mutex>
#include <string>
struct ma_engine;
struct ma_sound;
class AudioPlayer
{
public:
    AudioPlayer();
    ~AudioPlayer();
    AudioPlayer(const AudioPlayer &) = delete;
    AudioPlayer &operator=(const AudioPlayer &) = delete;
    bool initialize();
    bool load(const Song &song);
    void play();
    void pause();
    void stop();
    void seek(double seconds);
    void setVolume(float volume);
    bool isPlaying() const;
    bool isFinished() const;
    double position() const;
    double duration() const;
    float volume() const { return volume_; }
    const Song &currentSong() const { return currentSong_; }

private:
    ma_engine *engine_ = nullptr;
    ma_sound *sound_ = nullptr;
    Song currentSong_;
    float volume_ = 0.8f;
    mutable std::mutex mutex_;
};
