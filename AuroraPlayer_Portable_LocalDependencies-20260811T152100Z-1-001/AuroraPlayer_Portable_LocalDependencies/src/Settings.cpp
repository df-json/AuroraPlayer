#include "Settings.h"
#include <algorithm>
#include <cstdlib>
int Settings::playbackThreshold()const{try{return std::stoi(db_.setting("playback_threshold","30"));}catch(...){return 30;}}
float Settings::defaultVolume()const{try{return std::stof(db_.setting("default_volume","0.8"));}catch(...){return 0.8f;}}
void Settings::setPlaybackThreshold(int s){db_.setSetting("playback_threshold",std::to_string(std::max(0,s)));}void Settings::setDefaultVolume(float v){db_.setSetting("default_volume",std::to_string(std::clamp(v,0.0f,1.0f)));}
