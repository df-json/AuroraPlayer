#pragma once
#include "Song.h"
#include <cstddef>
#include <vector>
class Queue {
public:
    void clear(); void add(const Song& song); void addNext(const Song& song); bool remove(std::size_t index); void move(std::size_t from,std::size_t to);
    void setItems(const std::vector<Song>& items); const std::vector<Song>& items() const { return items_; }
    bool empty() const { return items_.empty(); } Song* current(); Song* next(); Song* previous(); void setIndex(std::size_t index); std::size_t index() const { return index_; }
private: std::vector<Song> items_; std::size_t index_=0;
};
