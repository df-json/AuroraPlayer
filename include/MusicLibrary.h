#pragma once
#include "Database.h"
#include <atomic>
#include <functional>
#include <string>
#include <thread>
class MusicLibrary {
public:
    explicit MusicLibrary(Database& database); ~MusicLibrary();
    bool addFolder(const std::string& path); bool removeFolder(const std::string& path);
    void scanAsync(std::function<void(int,int,const std::string&)> progress={}); void wait(); bool scanning() const { return scanning_; }
private: Database& db_; std::thread worker_; std::atomic<bool> scanning_{false}; void scan(std::function<void(int,int,const std::string&)> progress);
};
