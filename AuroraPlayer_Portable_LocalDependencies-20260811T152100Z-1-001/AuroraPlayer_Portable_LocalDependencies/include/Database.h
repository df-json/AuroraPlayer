#pragma once
#include "Song.h"
#include "Playlist.h"
#include "Album.h"
#include "Artist.h"
#include <sqlite3.h>
#include <optional>
#include <string>
#include <vector>
class Database
{
public:
    explicit Database(const std::string &path);
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;
    bool open();
    bool initialize();
    bool upsertSong(const Song &song);
    bool updateSongMetadata(const Song &song);
    std::vector<Song> songs() const;
    std::vector<Song> searchSongs(const std::string &query) const;
    std::optional<Song> songById(std::int64_t id) const;
    bool setLiked(std::int64_t id, bool liked);
    bool incrementPlayCount(std::int64_t id);
    bool addHistory(std::int64_t id, double seconds);
    std::vector<Song> likedSongs() const;
    std::vector<Song> recentSongs(int limit = 30) const;
    bool createPlaylist(const std::string &name, std::int64_t &id);
    bool renamePlaylist(std::int64_t id, const std::string &name);
    bool deletePlaylist(std::int64_t id);
    std::vector<Playlist> playlists() const;
    std::vector<Song> playlistSongs(std::int64_t playlistId) const;
    bool addSongToPlaylist(std::int64_t playlistId, std::int64_t songId);
    bool removeSongFromPlaylist(std::int64_t playlistId, std::int64_t songId);
    bool reorderPlaylist(std::int64_t playlistId, std::int64_t songId, std::int64_t newPosition);
    bool addLibraryFolder(const std::string &path);
    std::vector<std::string> libraryFolders() const;
    bool removeLibraryFolder(const std::string &path);
    bool reconcileMissingFiles();
    std::vector<Album> albums() const;
    std::optional<Album> album(const std::string &title, const std::string &artist) const;
    std::vector<Artist> artists() const;
    std::optional<Artist> artist(const std::string &name) const;
    bool setSetting(const std::string &key, const std::string &value);
    std::string setting(const std::string &key, const std::string &fallback = "") const;
    bool backup(const std::string &destination) const;
    bool restore(const std::string &source);
    const std::string &path() const { return path_; }

private:
    sqlite3 *db_ = nullptr;
    std::string path_;
    bool exec(const char *sql) const;
    static Song readSong(sqlite3_stmt *stmt);
    static std::string colText(sqlite3_stmt *stmt, int i);
};
