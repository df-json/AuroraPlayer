#include "Search.h"
#include <algorithm>
#include <cctype>
static std::string lower(std::string s){for(char&c:s)c=(char)std::tolower((unsigned char)c);return s;}
static int score(const Song&s,const std::string&q){auto t=lower(s.title),a=lower(s.artist),al=lower(s.album),g=lower(s.genre);if(t==q)return 1000;if(t.rfind(q,0)==0)return 850;if(t.find(q)!=std::string::npos)return 700;if(a==q)return 650;if(a.find(q)!=std::string::npos)return 550;if(al.find(q)!=std::string::npos)return 450;if(g.find(q)!=std::string::npos)return 300;return 0;}
std::vector<Song> Search::rank(const std::vector<Song>&src,const std::string&q){if(q.empty())return src;auto x=lower(q);std::vector<std::pair<int,Song>>v;for(const auto&s:src){int n=score(s,x);if(n)v.push_back({n,s});}std::stable_sort(v.begin(),v.end(),[](const auto&a,const auto&b){return a.first>b.first;});std::vector<Song>o;o.reserve(v.size());for(auto&x:v)o.push_back(std::move(x.second));return o;}
