#include "Database.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <ctime>

Database::Database(const std::string &path) : path_(path) {}
Database::~Database()
{
    if (db_)
        sqlite3_close(db_);
}
std::string Database::colText(sqlite3_stmt *s, int i)
{
    const auto *p = sqlite3_column_text(s, i);
    return p ? reinterpret_cast<const char *>(p) : "";
}
bool Database::open()
{
    auto parent = std::filesystem::path(path_).parent_path();
    if (!parent.empty())
        std::filesystem::create_directories(parent);
    if (sqlite3_open(path_.c_str(), &db_) != SQLITE_OK)
    {
        if (db_)
        {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }
    sqlite3_busy_timeout(db_, 3000);
    return true;
}
bool Database::exec(const char *sql) const
{
    char *err = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK)
    {
        std::cerr << "SQLite: " << (err ? err : "unknown") << '\n';
        sqlite3_free(err);
        return false;
    }
    return true;
}
bool Database::initialize()
{
    return exec("PRAGMA foreign_keys=ON;") && exec(R"SQL(
CREATE TABLE IF NOT EXISTS songs(id INTEGER PRIMARY KEY,title TEXT NOT NULL,artist TEXT NOT NULL,album TEXT NOT NULL,album_artist TEXT,genre TEXT,year INTEGER DEFAULT 0,track_number INTEGER DEFAULT 0,duration REAL DEFAULT 0,file_path TEXT UNIQUE NOT NULL,artwork_path TEXT,date_added INTEGER NOT NULL,play_count INTEGER DEFAULT 0,liked INTEGER DEFAULT 0,available INTEGER DEFAULT 1);
CREATE INDEX IF NOT EXISTS idx_song_title ON songs(title COLLATE NOCASE); CREATE INDEX IF NOT EXISTS idx_song_artist ON songs(artist COLLATE NOCASE); CREATE INDEX IF NOT EXISTS idx_song_album ON songs(album COLLATE NOCASE); CREATE INDEX IF NOT EXISTS idx_song_path ON songs(file_path);
CREATE TABLE IF NOT EXISTS playlists(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL UNIQUE,description TEXT DEFAULT '',artwork TEXT DEFAULT '',created_at INTEGER NOT NULL,updated_at INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS playlist_songs(playlist_id INTEGER NOT NULL,song_id INTEGER NOT NULL,position INTEGER NOT NULL,PRIMARY KEY(playlist_id,song_id),FOREIGN KEY(playlist_id) REFERENCES playlists(id) ON DELETE CASCADE,FOREIGN KEY(song_id) REFERENCES songs(id) ON DELETE CASCADE);
CREATE INDEX IF NOT EXISTS idx_playlist_position ON playlist_songs(playlist_id,position);
CREATE TABLE IF NOT EXISTS play_history(id INTEGER PRIMARY KEY AUTOINCREMENT,song_id INTEGER NOT NULL,played_at INTEGER NOT NULL,played_seconds REAL NOT NULL,FOREIGN KEY(song_id) REFERENCES songs(id) ON DELETE CASCADE);
CREATE INDEX IF NOT EXISTS idx_history_time ON play_history(played_at DESC);
CREATE TABLE IF NOT EXISTS library_folders(path TEXT PRIMARY KEY); CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY,value TEXT NOT NULL);
)SQL");
}
Song Database::readSong(sqlite3_stmt *s)
{
    Song x;
    int c = 0;
    x.id = sqlite3_column_int64(s, c++);
    x.title = colText(s, c++);
    x.artist = colText(s, c++);
    x.album = colText(s, c++);
    x.albumArtist = colText(s, c++);
    x.genre = colText(s, c++);
    x.year = sqlite3_column_int(s, c++);
    x.trackNumber = sqlite3_column_int(s, c++);
    x.duration = sqlite3_column_double(s, c++);
    x.filePath = colText(s, c++);
    x.artworkPath = colText(s, c++);
    x.dateAdded = sqlite3_column_int64(s, c++);
    x.playCount = sqlite3_column_int64(s, c++);
    x.liked = sqlite3_column_int(s, c++) != 0;
    x.available = sqlite3_column_int(s, c++) != 0;
    return x;
}
static const char *songSelect = "SELECT id,title,artist,album,album_artist,genre,year,track_number,duration,file_path,artwork_path,date_added,play_count,liked,available FROM songs";
bool Database::upsertSong(const Song &s)
{
    const char *sql = "INSERT INTO songs(title,artist,album,album_artist,genre,year,track_number,duration,file_path,artwork_path,date_added,play_count,liked,available) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(file_path) DO UPDATE SET title=excluded.title,artist=excluded.artist,album=excluded.album,album_artist=excluded.album_artist,genre=excluded.genre,year=excluded.year,track_number=excluded.track_number,duration=excluded.duration,artwork_path=excluded.artwork_path,available=1";
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, s.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, s.artist.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, s.album.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, s.albumArtist.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 5, s.genre.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 6, s.year);
    sqlite3_bind_int(st, 7, s.trackNumber);
    sqlite3_bind_double(st, 8, s.duration);
    sqlite3_bind_text(st, 9, s.filePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 10, s.artworkPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 11, s.dateAdded ? s.dateAdded : std::time(nullptr));
    sqlite3_bind_int64(st, 12, s.playCount);
    sqlite3_bind_int(st, 13, s.liked);
    sqlite3_bind_int(st, 14, s.available);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}
bool Database::updateSongMetadata(const Song &s)
{
    const char *sql = "UPDATE songs SET title=?,artist=?,album=?,album_artist=?,genre=?,year=?,track_number=?,artwork_path=? WHERE id=?";
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, s.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, s.artist.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, s.album.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, s.albumArtist.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 5, s.genre.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 6, s.year);
    sqlite3_bind_int(st, 7, s.trackNumber);
    sqlite3_bind_text(st, 8, s.artworkPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 9, s.id);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}
std::vector<Song> Database::songs() const
{
    std::vector<Song> v;
    std::string sql = songSelect;
    sql += " ORDER BY title COLLATE NOCASE,artist COLLATE NOCASE";
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr) != SQLITE_OK)
        return v;
    while (sqlite3_step(st) == SQLITE_ROW)
        v.push_back(readSong(st));
    sqlite3_finalize(st);
    return v;
}
std::vector<Song> Database::searchSongs(const std::string &q) const
{
    std::vector<Song> v;
    std::string sql = songSelect + std::string(" WHERE title LIKE ? COLLATE NOCASE OR artist LIKE ? COLLATE NOCASE OR album LIKE ? COLLATE NOCASE OR genre LIKE ? COLLATE NOCASE ORDER BY title COLLATE NOCASE");
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr) != SQLITE_OK)
        return v;
    std::string p = "%" + q + "%";
    for (int i = 1; i <= 4; i++)
        sqlite3_bind_text(st, i, p.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW)
        v.push_back(readSong(st));
    sqlite3_finalize(st);
    return v;
}
std::optional<Song> Database::songById(std::int64_t id) const
{
    std::string sql = songSelect + " WHERE id=?";
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr) != SQLITE_OK)
        return {};
    sqlite3_bind_int64(st, 1, id);
    std::optional<Song> r;
    if (sqlite3_step(st) == SQLITE_ROW)
        r = readSong(st);
    sqlite3_finalize(st);
    return r;
}
bool Database::setLiked(std::int64_t id, bool liked)
{
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, "UPDATE songs SET liked=? WHERE id=?", -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int(st, 1, liked);
    sqlite3_bind_int64(st, 2, id);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}
bool Database::incrementPlayCount(std::int64_t id)
{
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, "UPDATE songs SET play_count=play_count+1 WHERE id=?", -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int64(st, 1, id);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}
bool Database::addHistory(std::int64_t id, double seconds)
{
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, "INSERT INTO play_history(song_id,played_at,played_seconds) VALUES(?,?,?)", -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int64(st, 1, id);
    sqlite3_bind_int64(st, 2, std::time(nullptr));
    sqlite3_bind_double(st, 3, seconds);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}
std::vector<Song> Database::likedSongs() const
{
    std::string sql = songSelect + " WHERE liked=1 AND available=1 ORDER BY title COLLATE NOCASE";
    std::vector<Song> v;
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr) != SQLITE_OK)
        return v;
    while (sqlite3_step(st) == SQLITE_ROW)
        v.push_back(readSong(st));
    sqlite3_finalize(st);
    return v;
}
std::vector<Song> Database::recentSongs(int limit) const
{
    std::string sql = "SELECT " + std::string("s.id,s.title,s.artist,s.album,s.album_artist,s.genre,s.year,s.track_number,s.duration,s.file_path,s.artwork_path,s.date_added,s.play_count,s.liked,s.available") + " FROM play_history h JOIN songs s ON s.id=h.song_id WHERE s.available=1 ORDER BY h.played_at DESC LIMIT ?";
    std::vector<Song> v;
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr) != SQLITE_OK)
        return v;
    sqlite3_bind_int(st, 1, limit);
    while (sqlite3_step(st) == SQLITE_ROW)
        v.push_back(readSong(st));
    sqlite3_finalize(st);
    return v;
}
bool Database::createPlaylist(const std::string &name, std::int64_t &id)
{
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, "INSERT INTO playlists(name,created_at,updated_at) VALUES(?,?,?)", -1, &st, nullptr) != SQLITE_OK)
        return false;
    auto now = std::time(nullptr);
    sqlite3_bind_text(st, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 2, now);
    sqlite3_bind_int64(st, 3, now);
    if (sqlite3_step(st) != SQLITE_DONE)
    {
        sqlite3_finalize(st);
        return false;
    }
    id = sqlite3_last_insert_rowid(db_);
    sqlite3_finalize(st);
    return true;
}
bool Database::renamePlaylist(std::int64_t id, const std::string &name)
{
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, "UPDATE playlists SET name=?,updated_at=? WHERE id=?", -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 2, std::time(nullptr));
    sqlite3_bind_int64(st, 3, id);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}
bool Database::deletePlaylist(std::int64_t id)
{
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM playlists WHERE id=?", -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int64(st, 1, id);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}
std::vector<Playlist> Database::playlists() const
{
    std::vector<Playlist> v;
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT id,name,description,artwork,created_at,updated_at FROM playlists ORDER BY name COLLATE NOCASE", -1, &st, nullptr) != SQLITE_OK)
        return v;
    while (sqlite3_step(st) == SQLITE_ROW)
    {
        Playlist p;
        p.id = sqlite3_column_int64(st, 0);
        p.name = colText(st, 1);
        p.description = colText(st, 2);
        p.artwork = colText(st, 3);
        p.createdAt = sqlite3_column_int64(st, 4);
        p.updatedAt = sqlite3_column_int64(st, 5);
        v.push_back(p);
    }
    sqlite3_finalize(st);
    return v;
}
std::vector<Song> Database::playlistSongs(std::int64_t pid) const
{
    std::string sql = "SELECT s.id,s.title,s.artist,s.album,s.album_artist,s.genre,s.year,s.track_number,s.duration,s.file_path,s.artwork_path,s.date_added,s.play_count,s.liked,s.available FROM playlist_songs p JOIN songs s ON s.id=p.song_id WHERE p.playlist_id=? ORDER BY p.position";
    std::vector<Song> v;
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr) != SQLITE_OK)
        return v;
    sqlite3_bind_int64(st, 1, pid);
    while (sqlite3_step(st) == SQLITE_ROW)
        v.push_back(readSong(st));
    sqlite3_finalize(st);
    return v;
}
bool Database::addSongToPlaylist(std::int64_t pid, std::int64_t sid)
{
    const char *sql = "INSERT OR IGNORE INTO playlist_songs(playlist_id,song_id,position) VALUES(?,?,COALESCE((SELECT MAX(position)+1 FROM playlist_songs WHERE playlist_id=?),0))";
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int64(st, 1, pid);
    sqlite3_bind_int64(st, 2, sid);
    sqlite3_bind_int64(st, 3, pid);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}
bool Database::removeSongFromPlaylist(std::int64_t pid, std::int64_t sid)
{
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM playlist_songs WHERE playlist_id=? AND song_id=?", -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int64(st, 1, pid);
    sqlite3_bind_int64(st, 2, sid);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}
bool Database::reorderPlaylist(std::int64_t pid, std::int64_t sid, std::int64_t newPosition)
{
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT position FROM playlist_songs WHERE playlist_id=? AND song_id=?", -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int64(st, 1, pid);
    sqlite3_bind_int64(st, 2, sid);
    if (sqlite3_step(st) != SQLITE_ROW)
    {
        sqlite3_finalize(st);
        return false;
    }
    auto old = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    if (old == newPosition)
        return true;
    if (newPosition < 0)
        newPosition = 0;
    exec("BEGIN IMMEDIATE;");
    const char *shift = newPosition < old ? "UPDATE playlist_songs SET position=position+1 WHERE playlist_id=? AND position>=? AND position<? AND song_id<>?" : "UPDATE playlist_songs SET position=position-1 WHERE playlist_id=? AND position>? AND position<=? AND song_id<>?";
    if (sqlite3_prepare_v2(db_, shift, -1, &st, nullptr) != SQLITE_OK)
    {
        exec("ROLLBACK;");
        return false;
    }
    sqlite3_bind_int64(st, 1, pid);
    sqlite3_bind_int64(st, 2, newPosition < old ? newPosition : old);
    sqlite3_bind_int64(st, 3, newPosition < old ? old : newPosition);
    sqlite3_bind_int64(st, 4, sid);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    if (ok && sqlite3_prepare_v2(db_, "UPDATE playlist_songs SET position=? WHERE playlist_id=? AND song_id=?", -1, &st) == SQLITE_OK)
    {
        sqlite3_bind_int64(st, 1, newPosition);
        sqlite3_bind_int64(st, 2, pid);
        sqlite3_bind_int64(st, 3, sid);
        ok = sqlite3_step(st) == SQLITE_DONE;
        sqlite3_finalize(st);
    }
    else
        ok = false;
    if (ok)
        exec("COMMIT;");
    else
        exec("ROLLBACK;");
    return ok;
}
bool Database::addLibraryFolder(const std::string &p)
{
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, "INSERT OR IGNORE INTO library_folders(path) VALUES(?)", -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, p.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}
std::vector<std::string> Database::libraryFolders() const
{
    std::vector<std::string> v;
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT path FROM library_folders ORDER BY path", -1, &st, nullptr) != SQLITE_OK)
        return v;
    while (sqlite3_step(st) == SQLITE_ROW)
        v.push_back(colText(st, 0));
    sqlite3_finalize(st);
    return v;
}
bool Database::removeLibraryFolder(const std::string &p)
{
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM library_folders WHERE path=?", -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, p.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}
bool Database::reconcileMissingFiles()
{
    return exec("UPDATE songs SET available=0 WHERE file_path NOT NULL") && [&]
    {sqlite3_stmt*st=nullptr;if(sqlite3_prepare_v2(db_,"SELECT id,file_path FROM songs",-1,&st,nullptr)!=SQLITE_OK)return false;sqlite3_stmt*u=nullptr;if(sqlite3_prepare_v2(db_,"UPDATE songs SET available=? WHERE id=?",-1,&u,nullptr)!=SQLITE_OK){sqlite3_finalize(st);return false;}while(sqlite3_step(st)==SQLITE_ROW){bool ok=std::filesystem::exists(colText(st,1));sqlite3_bind_int(u,1,ok);sqlite3_bind_int64(u,2,sqlite3_column_int64(st,0));sqlite3_step(u);sqlite3_reset(u);}sqlite3_finalize(u);sqlite3_finalize(st);return true; }();
}
std::vector<Album> Database::albums() const
{
    std::vector<Album> v;
    sqlite3_stmt *st = nullptr;
    const char *sql = "SELECT album,COALESCE(NULLIF(album_artist,''),artist),MAX(year),MAX(artwork_path) FROM songs WHERE available=1 GROUP BY album,COALESCE(NULLIF(album_artist,''),artist) ORDER BY album COLLATE NOCASE";
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return v;
    while (sqlite3_step(st) == SQLITE_ROW)
    {
        Album a;
        a.title = colText(st, 0);
        a.artist = colText(st, 1);
        a.year = sqlite3_column_int(st, 2);
        a.artworkPath = colText(st, 3);
        v.push_back(std::move(a));
    }
    sqlite3_finalize(st);
    for (auto &a : v)
        a.songs = album(a.title, a.artist)->songs;
    return v;
}
std::optional<Album> Database::album(const std::string &t, const std::string &a) const
{
    Album x;
    x.title = t;
    x.artist = a;
    std::string sql = songSelect + " WHERE album=? AND COALESCE(NULLIF(album_artist,''),artist)=? AND available=1 ORDER BY track_number,title COLLATE NOCASE";
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr) != SQLITE_OK)
        return {};
    sqlite3_bind_text(st, 1, t.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, a.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW)
        x.songs.push_back(readSong(st));
    sqlite3_finalize(st);
    if (x.songs.empty())
        return {};
    x.year = x.songs.front().year;
    x.artworkPath = x.songs.front().artworkPath;
    return x;
}
std::vector<Artist> Database::artists() const
{
    std::vector<Artist> v;
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COALESCE(NULLIF(album_artist,''),artist),MAX(artwork_path) FROM songs WHERE available=1 GROUP BY COALESCE(NULLIF(album_artist,''),artist) ORDER BY 1 COLLATE NOCASE", -1, &st, nullptr) != SQLITE_OK)
        return v;
    while (sqlite3_step(st) == SQLITE_ROW)
    {
        Artist a;
        a.name = colText(st, 0);
        a.artworkPath = colText(st, 1);
        v.push_back(std::move(a));
    }
    sqlite3_finalize(st);
    for (auto &a : v)
    {
        auto x = artist(a.name);
        if (x)
            a.songs = x->songs;
    }
    return v;
}
std::optional<Artist> Database::artist(const std::string &name) const
{
    Artist a;
    a.name = name;
    std::string sql = songSelect + " WHERE COALESCE(NULLIF(album_artist,''),artist)=? AND available=1 ORDER BY album COLLATE NOCASE,track_number,title COLLATE NOCASE";
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st, nullptr) != SQLITE_OK)
        return {};
    sqlite3_bind_text(st, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW)
        a.songs.push_back(readSong(st));
    sqlite3_finalize(st);
    if (a.songs.empty())
        return {};
    a.artworkPath = a.songs.front().artworkPath;
    return a;
}
bool Database::setSetting(const std::string &k, const std::string &v)
{
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, "INSERT INTO settings(key,value) VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value", -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, k.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, v.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}
std::string Database::setting(const std::string &k, const std::string &fallback) const
{
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT value FROM settings WHERE key=?", -1, &st, nullptr) != SQLITE_OK)
        return fallback;
    sqlite3_bind_text(st, 1, k.c_str(), -1, SQLITE_TRANSIENT);
    std::string v = fallback;
    if (sqlite3_step(st) == SQLITE_ROW)
        v = colText(st, 0);
    sqlite3_finalize(st);
    return v;
}
bool Database::backup(const std::string &destination) const
{
    auto parent = std::filesystem::path(destination).parent_path();
    if (!parent.empty())
        std::filesystem::create_directories(parent);
    sqlite3 *dst = nullptr;
    if (sqlite3_open(destination.c_str(), &dst) != SQLITE_OK)
        return false;
    sqlite3_backup *b = sqlite3_backup_init(dst, "main", db_, "main");
    bool ok = false;
    if (b)
    {
        sqlite3_backup_step(b, -1);
        ok = sqlite3_backup_finish(b) == SQLITE_OK;
    }
    sqlite3_close(dst);
    return ok;
}

bool Database::restore(const std::string &source)
{
    sqlite3 *src = nullptr;
    if (sqlite3_open(source.c_str(), &src) != SQLITE_OK)
    {
        if (src)
            sqlite3_close(src);
        return false;
    }
    sqlite3_backup *b = sqlite3_backup_init(db_, "main", src, "main");
    bool ok = false;
    if (b)
    {
        sqlite3_backup_step(b, -1);
        ok = sqlite3_backup_finish(b) == SQLITE_OK;
    }
    sqlite3_close(src);
    return ok;
}
