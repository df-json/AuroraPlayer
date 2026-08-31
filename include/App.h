#pragma once
#include "AudioPlayer.h"
#include "Database.h"
#include "MusicLibrary.h"
#include "Queue.h"
#include "Settings.h"
#include "UI.h"
#include <memory>
#include <SDL.h>
class App { public: App()=default; ~App(); bool initialize(); int run(); private: SDL_Window* window_=nullptr; SDL_Renderer* renderer_=nullptr; std::unique_ptr<Database> db_; std::unique_ptr<MusicLibrary> library_; std::unique_ptr<AudioPlayer> player_; std::unique_ptr<Queue> queue_; std::unique_ptr<Settings> settings_; std::unique_ptr<UI> ui_; };
