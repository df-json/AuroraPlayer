#pragma once
#include "AudioPlayer.h"
#include "Database.h"
#include "MusicLibrary.h"
#include "Queue.h"
#include "Settings.h"
#include <SDL.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>
class UI {
public:
    UI(Database& db,MusicLibrary& library,AudioPlayer& player,Queue& queue,Settings& settings);
    ~UI();
    void render(); bool wantsQuit() const { return quit_; } void setWindow(SDL_Window* window){window_=window;}
public:
    void playSong(const Song& song);
    void openMetadata(const Song& song);
private:
    Database& db_; MusicLibrary& library_; AudioPlayer& player_; Queue& queue_; Settings& settings_; SDL_Window* window_=nullptr;
    bool quit_=false; int page_=0; int libraryTab_=0; std::string searchQuery_; std::string selectedFolder_; std::int64_t selectedPlaylist_=0; std::string selectedAlbumTitle_; std::string selectedAlbumArtist_; std::string selectedArtistName_; std::unordered_map<std::string,SDL_Texture*> textures_;
    int sortMode_=0; int themeMode_=0; float uiScale_=1.0f; bool showMetadata_=false; Song editingSong_{}; bool showQueue_=false; bool showAbout_=false; bool shuffle_=false; int repeatMode_=0; std::vector<Song> playOrder_; std::size_t playOrderIndex_=0; std::int64_t trackedSongId_=0; double trackedStart_=0; std::atomic<int> scanDone_{0},scanTotal_{0};
    void renderSidebar(); void renderHome(); void renderLibrary(); void renderSearch(); void renderPlaylists(); void renderLiked(); void renderRecent(); void renderQueue(); void renderSettings(); void renderAlbum(const Album&); void renderArtist(const Artist&); void renderPlayerBar();
    void advance(bool userRequested); void previous(); void rebuildShuffle(const std::vector<Song>& base); void handlePlaybackProgress(); void renderMetadataDialog(); SDL_Texture* textureFor(const std::string& path); void clearTextures();
};
