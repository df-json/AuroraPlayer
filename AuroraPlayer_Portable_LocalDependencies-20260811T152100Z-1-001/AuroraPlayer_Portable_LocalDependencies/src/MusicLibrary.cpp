#include "MusicLibrary.h"
#include "Metadata.h"
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <vector>
namespace fs=std::filesystem;
static bool audioFile(const fs::path&p){auto e=p.extension().string();std::transform(e.begin(),e.end(),e.begin(),[](unsigned char c){return(char)std::tolower(c);});return e==".mp3"||e==".wav"||e==".flac"||e==".ogg"||e==".oga"||e==".m4a"||e==".aac"||e==".mp4";}
MusicLibrary::MusicLibrary(Database&d):db_(d){} MusicLibrary::~MusicLibrary(){wait();}
bool MusicLibrary::addFolder(const std::string&p){std::error_code ec;if(!fs::is_directory(p,ec))return false;return db_.addLibraryFolder(fs::weakly_canonical(p,ec).string());}bool MusicLibrary::removeFolder(const std::string&p){return db_.removeLibraryFolder(p);}void MusicLibrary::wait(){if(worker_.joinable())worker_.join();}
void MusicLibrary::scanAsync(std::function<void(int,int,const std::string&)>progress){if(scanning_)return;wait();scanning_=true;worker_=std::thread([this,progress]{scan(progress);scanning_=false;});}
void MusicLibrary::scan(std::function<void(int,int,const std::string&)>progress){std::vector<fs::path>files;for(const auto&root:db_.libraryFolders()){std::error_code ec;if(!fs::exists(root,ec))continue;fs::recursive_directory_iterator it(root,fs::directory_options::skip_permission_denied,ec),end;for(;it!=end;it.increment(ec)){if(ec){ec.clear();continue;}if(it->is_regular_file(ec)&&audioFile(it->path()))files.push_back(it->path());}}int total=(int)files.size(),done=0;for(const auto&p:files){Song s=MetadataReader::read(p.string());db_.upsertSong(s);++done;if(progress)progress(done,total,p.string());}db_.reconcileMissingFiles();}
