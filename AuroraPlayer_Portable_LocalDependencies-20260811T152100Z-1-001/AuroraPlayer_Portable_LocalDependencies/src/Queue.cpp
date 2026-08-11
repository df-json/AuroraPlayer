#include "Queue.h"
#include <algorithm>
void Queue::clear()
{
    items_.clear();
    index_ = 0;
}
void Queue::add(const Song &s)
{
    items_.push_back(s);
    if (items_.size() == 1)
        index_ = 0;
}
void Queue::addNext(const Song &s)
{
    auto p = items_.empty() ? 0 : std::min(index_ + 1, items_.size());
    items_.insert(items_.begin() + static_cast<std::ptrdiff_t>(p), s);
}
bool Queue::remove(std::size_t i)
{
    if (i >= items_.size())
        return false;
    items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(i));
    if (items_.empty())
    {
        index_ = 0;
        return true;
    }
    if (i < index_ && index_ > 0)
        --index_;
    else if (index_ >= items_.size())
        index_ = items_.size() - 1;
    return true;
}
void Queue::move(std::size_t a, std::size_t b)
{
    if (a >= items_.size() || b >= items_.size() || a == b)
        return;
    auto s = items_[a];
    items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(a));
    items_.insert(items_.begin() + static_cast<std::ptrdiff_t>(b), s);
    if (index_ == a)
        index_ = b;
}
void Queue::setItems(const std::vector<Song> &v)
{
    items_ = v;
    index_ = items_.empty() ? 0 : 0;
}
void Queue::setIndex(std::size_t i)
{
    if (!items_.empty())
        index_ = std::min(i, items_.size() - 1);
    else
        index_ = 0;
}
Song *Queue::current() { return items_.empty() ? nullptr : &items_[index_]; }
Song *Queue::next()
{
    if (items_.empty() || index_ + 1 >= items_.size())
        return nullptr;
    return &items_[++index_];
}
Song *Queue::previous()
{
    if (items_.empty() || index_ == 0)
        return nullptr;
    return &items_[--index_];
}
