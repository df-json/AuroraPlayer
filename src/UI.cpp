#include "UI.h"
#include "Search.h"
#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static bool folderDialog(std::string&out){
#ifdef _WIN32
    BROWSEINFOW bi{};bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;bi.lpszTitle=L"Choose your music folder";PIDLIST_ABSOLUTE pid=SHBrowseForFolderW(&bi);if(!pid)return false;wchar_t path[MAX_PATH];bool ok=SHGetPathFromIDListW(pid,path);CoTaskMemFree(pid);if(!ok)return false;int n=WideCharToMultiByte(CP_UTF8,0,path,-1,nullptr,0,nullptr,nullptr);std::string s(n-1,'\0');WideCharToMultiByte(CP_UTF8,0,path,-1,s.data(),n,nullptr,nullptr);out=s;return true;
#else
    return false;
#endif
}
static std::string timeFmt(double sec){int s=std::max(0,(int)sec);return std::to_string(s/60)+":"+(s%60<10?"0":"")+std::to_string(s%60);}
// Fixed height reserved for the player bar at the bottom of the window. Both the
// sidebar and the content area must stop this far above the bottom (not just the
// content area) — otherwise the sidebar, which has no bottom limit of its own, fills
// the whole window and pushes the player bar below the visible area, so it only
// becomes visible if you scroll the window down. Bumped it up a bit from the old
// implicit ~100px so the larger icon buttons below have room to breathe.
static constexpr float kPlayerBarHeight=108.0f;
UI::UI(Database&d,MusicLibrary&l,AudioPlayer&p,Queue&q,Settings&s):db_(d),library_(l),player_(p),queue_(q),settings_(s){}
UI::~UI(){clearTextures();}
SDL_Texture* UI::textureFor(const std::string&path){if(path.empty()||!window_)return nullptr;auto it=textures_.find(path);if(it!=textures_.end())return it->second;int w=0,h=0,n=0;unsigned char*data=stbi_load(path.c_str(),&w,&h,&n,4);if(!data)return nullptr;SDL_Renderer*r=nullptr;SDL_GetWindowData(window_,"AuroraRenderer");/* renderer is stored by SDL window user data in App below when available */r=(SDL_Renderer*)SDL_GetWindowData(window_,"renderer");if(!r){stbi_image_free(data);return nullptr;}SDL_Surface*surf=SDL_CreateRGBSurfaceWithFormatFrom(data,w,h,32,w*4,SDL_PIXELFORMAT_RGBA32);if(!surf){stbi_image_free(data);return nullptr;}SDL_Texture*t=SDL_CreateTextureFromSurface(r,surf);SDL_FreeSurface(surf);stbi_image_free(data);if(t)textures_[path]=t;return t;}
void UI::clearTextures(){for(auto&x:textures_)if(x.second)SDL_DestroyTexture(x.second);textures_.clear();}
void UI::playSong(const Song&s){if(!s.available||!std::filesystem::exists(s.filePath))return;if(player_.load(s)){player_.play();trackedSongId_=s.id;trackedStart_=0;}}
void UI::rebuildShuffle(const std::vector<Song>&base){playOrder_=base;std::mt19937 rng((unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count());std::shuffle(playOrder_.begin(),playOrder_.end(),rng);playOrderIndex_=0;}
void UI::advance(bool userRequested){auto&v=queue_.items();if(!v.empty()){if(auto*s=queue_.next()){playSong(*s);return;}}
    std::vector<Song>base=db_.songs();base.erase(std::remove_if(base.begin(),base.end(),[](const Song&s){return !s.available;}),base.end());if(base.empty())return;
    if(repeatMode_==2&&!userRequested){playSong(player_.currentSong());return;}
    if(shuffle_){if(playOrder_.empty())rebuildShuffle(base);if(playOrderIndex_+1>=playOrder_.size()){if(repeatMode_==1)rebuildShuffle(base);else return;}else ++playOrderIndex_;if(!playOrder_.empty())playSong(playOrder_[playOrderIndex_]);return;}
    auto cur=player_.currentSong();auto it=std::find_if(base.begin(),base.end(),[&](const Song&s){return s.id==cur.id;});if(it==base.end()){playSong(base.front());return;}auto idx=(size_t)std::distance(base.begin(),it);if(idx+1>=base.size()){if(repeatMode_==1)playSong(base.front());}else playSong(base[idx+1]);}
void UI::previous(){if(player_.position()>3){player_.seek(0);return;}if(auto*s=queue_.previous()){playSong(*s);return;}auto base=db_.songs();auto cur=player_.currentSong();auto it=std::find_if(base.begin(),base.end(),[&](const Song&s){return s.id==cur.id;});if(it!=base.end()&&it!=base.begin())playSong(*std::prev(it));}
void UI::handlePlaybackProgress(){if(trackedSongId_==0)return;double p=player_.position();double d=player_.duration();if(p>=settings_.playbackThreshold()||(d>0&&p>=d*0.5)){if(trackedStart_==0){db_.addHistory(trackedSongId_,p);db_.incrementPlayCount(trackedSongId_);trackedStart_=1;}}if(player_.isFinished()){if(repeatMode_==2)advance(false);else advance(false);trackedSongId_=0;}}
enum class Icon{Prev,Play,Pause,Next,Shuffle,Repeat,RepeatOne,Queue,HeartFilled,HeartOutline,Plus};
static bool iconButton(const char*id,Icon icon,bool active,const char*tooltip,float size=34.0f);
static std::string ellipsize(const std::string&s,float maxWidth){
    if(ImGui::CalcTextSize(s.c_str()).x<=maxWidth)return s;
    std::string out=s;
    while(!out.empty()&&ImGui::CalcTextSize((out+"...").c_str()).x>maxWidth)out.pop_back();
    return out.empty()?out:out+"...";
}
// Column X positions/widths for song rows. Text longer than its column's width gets
// ellipsized (see ellipsize() above) instead of being left to run into the next
// column — with SameLine(x) placing the *next* column at a fixed X regardless of how
// wide the previous cell's text actually rendered, a long album name (e.g. "Being
// Funny In A Foreign Language") would otherwise visually collide with the duration
// column right after it.
static constexpr float kColArtist=300.0f,kColAlbum=520.0f,kColDuration=790.0f;
static bool songRow(const Song&s,UI&ui,Database&db,Queue&q,const std::int64_t*inPlaylist=nullptr){bool clicked=false;ImGui::PushID((int)s.id);if(!s.available)ImGui::BeginDisabled();
    // AllowOverlap: without this, Selectable's hit-rect spans the ENTIRE row width
    // (out to the far right edge) regardless of its visible label — which sits
    // underneath the heart/add-to-queue buttons drawn after it on the same row and
    // was silently eating their clicks as "play this song" instead. This flag tells
    // ImGui those later, smaller widgets should get the click when hovered directly,
    // not the big Selectable behind them.
    if(ImGui::Selectable((ellipsize(s.title,kColArtist-10)+"##song").c_str(),false,ImGuiSelectableFlags_AllowDoubleClick|ImGuiSelectableFlags_AllowOverlap)){ui.playSong(s);clicked=true;}
    // BeginPopupContextItem() (used below to actually show the menu) keys off the
    // MOST RECENTLY SUBMITTED item — but several widgets (artist/album/duration text,
    // the heart and add-to-queue buttons) get drawn between here and that call, so it
    // was really only listening for right-clicks on the tiny plus button, not the row.
    // Capture the right-click on the row itself right here, immediately after the
    // Selectable, and open the popup explicitly instead.
    if(ImGui::IsItemClicked(ImGuiMouseButton_Right))ImGui::OpenPopup("menu");
    ImGui::SameLine(kColArtist);ImGui::TextDisabled("%s",ellipsize(s.artist,kColAlbum-kColArtist-10).c_str());
    ImGui::SameLine(kColAlbum);ImGui::TextDisabled("%s",ellipsize(s.album,kColDuration-kColAlbum-10).c_str());
    ImGui::SameLine(kColDuration);ImGui::TextDisabled("%s",timeFmt(s.duration).c_str());
    ImGui::SameLine();if(iconButton("##like",s.liked?Icon::HeartFilled:Icon::HeartOutline,s.liked,s.liked?"Unlike":"Like",22.0f))db.setLiked(s.id,!s.liked);ImGui::SameLine();if(iconButton("##addq",Icon::Plus,false,"Add to queue",22.0f))q.add(s);if(ImGui::BeginPopup("menu")){if(ImGui::MenuItem("Play"))ui.playSong(s);if(ImGui::MenuItem("Play Next"))q.addNext(s);if(ImGui::MenuItem("Add to Queue"))q.add(s);if(ImGui::MenuItem(s.liked?"Unlike":"Like"))db.setLiked(s.id,!s.liked);if(ImGui::MenuItem("Edit Metadata"))ui.openMetadata(s);if(ImGui::MenuItem("Open File Location")){
#ifdef _WIN32
 std::string cmd="explorer.exe /select,\""+s.filePath+"\""; std::system(cmd.c_str());
#endif
}if(ImGui::BeginMenu("Add to Playlist")){for(auto&p:db.playlists())if(ImGui::MenuItem(p.name.c_str()))db.addSongToPlaylist(p.id,s.id);ImGui::EndMenu();}if(inPlaylist&&ImGui::MenuItem("Remove from This Playlist"))db.removeSongFromPlaylist(*inPlaylist,s.id);ImGui::EndPopup();}if(!s.available)ImGui::EndDisabled();ImGui::PopID();return clicked;}
void UI::renderSidebar(){ImGui::BeginChild("sidebar",ImVec2(220,-kPlayerBarHeight),true);ImGui::TextColored(ImVec4(.45f,.85f,.65f,1),"AURORA");ImGui::TextDisabled("LOCAL MUSIC PLAYER");ImGui::Separator();const char*items[]={"Home","Search","Library","Playlists","Liked Songs","Recently Played","Queue","Settings"};for(int i=0;i<8;i++)if(ImGui::Selectable(items[i],page_==i,0,ImVec2(0,40)))page_=i;ImGui::Dummy(ImVec2(0,10));ImGui::TextDisabled("Offline • C++17");ImGui::EndChild();}
void UI::renderHome(){ImGui::Text("Good morning");ImGui::TextDisabled("Your music. Your files. No account required.");ImGui::Separator();ImGui::Text("Recently Played");auto recent=db_.recentSongs(8);ImGui::PushID("recent");for(auto&s:recent)songRow(s,*this,db_,queue_);ImGui::PopID();if(recent.empty())ImGui::TextDisabled("Play a song to build your history.");ImGui::Spacing();ImGui::Text("Recently Added");auto all=db_.songs();ImGui::PushID("added");int n=0;for(auto&s:all){if(n++>=8)break;songRow(s,*this,db_,queue_);}ImGui::PopID();if(all.empty())ImGui::TextDisabled("Add a music folder from Settings.");}
void UI::renderSearch(){ImGui::Text("Search your library");char buf[512];std::snprintf(buf,sizeof(buf),"%s",searchQuery_.c_str());ImGui::SetNextItemWidth(-1);if(ImGui::InputTextWithHint("##search","Songs, artists, albums...",buf,sizeof(buf)))searchQuery_=buf;ImGui::Separator();auto results=Search::rank(db_.songs(),searchQuery_);for(auto&s:results)songRow(s,*this,db_,queue_);if(!searchQuery_.empty()&&results.empty())ImGui::TextDisabled("No matches.");}
void UI::renderLibrary(){ImGui::Text("Your Library");const char*tabs[]={"Songs","Albums","Artists"};for(int i=0;i<3;i++){if(i)ImGui::SameLine();if(ImGui::Selectable(tabs[i],libraryTab_==i,ImGuiSelectableFlags_None,ImVec2(100,30)))libraryTab_=i;}ImGui::Separator();if(libraryTab_==0){auto v=db_.songs();const char*sorts[]={"Name","Artist","Album","Date Added","Play Count"};ImGui::SetNextItemWidth(150);ImGui::Combo("Sort",&sortMode_,sorts,5);std::sort(v.begin(),v.end(),[&](const Song&a,const Song&b){if(sortMode_==1)return a.artist<b.artist;if(sortMode_==2)return a.album<b.album;if(sortMode_==3)return a.dateAdded>b.dateAdded;if(sortMode_==4)return a.playCount>b.playCount;return a.title<b.title;});ImGui::TextDisabled("%zu songs",v.size());for(auto&s:v)songRow(s,*this,db_,queue_);}else if(libraryTab_==1){for(auto&a:db_.albums()){ImGui::PushID(a.title.c_str());if(ImGui::Selectable((a.title+"##album").c_str())){selectedAlbumTitle_=a.title;selectedAlbumArtist_=a.artist;}ImGui::SameLine(350);ImGui::TextDisabled("%s • %d • %zu tracks",a.artist.c_str(),a.year,a.songs.size());ImGui::PopID();}if(!selectedAlbumTitle_.empty()){auto a=db_.album(selectedAlbumTitle_,selectedAlbumArtist_);if(a)renderAlbum(*a);}}else{for(auto&a:db_.artists()){if(ImGui::Selectable((a.name+"##artist").c_str()))selectedArtistName_=a.name;ImGui::SameLine(350);ImGui::TextDisabled("%zu songs",a.songs.size());}if(!selectedArtistName_.empty()){auto a=db_.artist(selectedArtistName_);if(a)renderArtist(*a);}}}
void UI::renderAlbum(const Album&a){ImGui::Separator();if(auto*t=textureFor(a.artworkPath)){ImGui::Image((ImTextureID)(intptr_t)t,ImVec2(110,110));ImGui::SameLine();}ImGui::Text("Album: %s",a.title.c_str());ImGui::TextDisabled("%s • %d • %zu tracks",a.artist.c_str(),a.year,a.songs.size());if(ImGui::Button("Play Album")){for(auto&s:a.songs)queue_.add(s);if(!a.songs.empty())playSong(a.songs.front());}ImGui::SameLine();if(ImGui::Button("Shuffle")){rebuildShuffle(a.songs);if(!playOrder_.empty())playSong(playOrder_[0]);}ImGui::SameLine();if(ImGui::Button("Add to Queue"))for(auto&s:a.songs)queue_.add(s);for(auto&s:a.songs)songRow(s,*this,db_,queue_);}
void UI::renderArtist(const Artist&a){ImGui::Separator();if(auto*t=textureFor(a.artworkPath)){ImGui::Image((ImTextureID)(intptr_t)t,ImVec2(110,110));ImGui::SameLine();}ImGui::Text("Artist: %s",a.name.c_str());if(ImGui::Button("Play All")){for(auto&s:a.songs)queue_.add(s);if(!a.songs.empty())playSong(a.songs.front());}ImGui::SameLine();if(ImGui::Button("Shuffle")){rebuildShuffle(a.songs);if(!playOrder_.empty())playSong(playOrder_[0]);}ImGui::SameLine();if(ImGui::Button("Add to Queue"))for(auto&s:a.songs)queue_.add(s);for(auto&s:a.songs)songRow(s,*this,db_,queue_);}
void UI::renderPlaylists(){ImGui::Text("Playlists");static char name[128]="";ImGui::SetNextItemWidth(280);ImGui::InputText("##newplaylist",name,sizeof(name));ImGui::SameLine();if(ImGui::Button("Create")&&name[0]){std::int64_t id;if(db_.createPlaylist(name,id))selectedPlaylist_=id;name[0]=0;}ImGui::Separator();for(auto&p:db_.playlists()){ImGui::PushID((int)p.id);if(ImGui::Selectable(p.name.c_str(),selectedPlaylist_==p.id,ImGuiSelectableFlags_AllowOverlap))selectedPlaylist_=p.id;ImGui::SameLine();if(ImGui::SmallButton("Delete")){db_.deletePlaylist(p.id);if(selectedPlaylist_==p.id)selectedPlaylist_=0;}ImGui::PopID();}if(selectedPlaylist_){auto ps=db_.playlistSongs(selectedPlaylist_);ImGui::Separator();if(ImGui::Button("Play Playlist")&&!ps.empty()){queue_.setItems(ps);playSong(ps.front());}ImGui::SameLine();if(ImGui::Button("Shuffle Playlist")&&!ps.empty()){rebuildShuffle(ps);playSong(playOrder_[0]);}ImGui::SameLine();if(ImGui::Button("Add to Queue"))for(auto&s:ps)queue_.add(s);for(size_t i=0;i<ps.size();++i){songRow(ps[i],*this,db_,queue_,&selectedPlaylist_);ImGui::SameLine();if(i>0&&ImGui::SmallButton(("Up##"+std::to_string(ps[i].id)).c_str()))db_.reorderPlaylist(selectedPlaylist_,ps[i].id,(std::int64_t)i-1);ImGui::SameLine();if(i+1<ps.size()&&ImGui::SmallButton(("Down##"+std::to_string(ps[i].id)).c_str()))db_.reorderPlaylist(selectedPlaylist_,ps[i].id,(std::int64_t)i+1);}}}
void UI::renderLiked(){ImGui::Text("Liked Songs");for(auto&s:db_.likedSongs())songRow(s,*this,db_,queue_);}
void UI::renderRecent(){ImGui::Text("Recently Played");for(auto&s:db_.recentSongs(100))songRow(s,*this,db_,queue_);}
void UI::renderQueue(){ImGui::Text("Queue");if(ImGui::Button("Clear Queue"))queue_.clear();ImGui::Separator();auto v=queue_.items();for(size_t i=0;i<v.size();++i){ImGui::PushID((int)i);ImGui::Text("%zu. %s — %s",i+1,v[i].title.c_str(),v[i].artist.c_str());ImGui::SameLine();if(ImGui::SmallButton("Play")){queue_.setIndex(i);playSong(v[i]);}ImGui::SameLine();if(ImGui::SmallButton("Remove"))queue_.remove(i);ImGui::SameLine();if(i>0&&ImGui::SmallButton("Up"))queue_.move(i,i-1);ImGui::SameLine();if(i+1<v.size()&&ImGui::SmallButton("Down"))queue_.move(i,i+1);ImGui::PopID();}}
void UI::renderSettings(){ImGui::Text("Settings");ImGui::Text("Music Library");for(auto&f:db_.libraryFolders()){ImGui::BulletText("%s",f.c_str());ImGui::SameLine();if(ImGui::SmallButton(("Remove##"+f).c_str()))library_.removeFolder(f);}if(ImGui::Button("Add Music Folder")){if(folderDialog(selectedFolder_)){library_.addFolder(selectedFolder_);scanDone_=0;scanTotal_=0;library_.scanAsync([this](int done,int total,const std::string&){scanDone_=done;scanTotal_=total;});}}ImGui::SameLine();if(ImGui::Button("Rescan")){scanDone_=0;scanTotal_=0;library_.scanAsync([this](int done,int total,const std::string&){scanDone_=done;scanTotal_=total;});}ImGui::TextDisabled("Scanning: %s",library_.scanning()?"yes":"no");if(scanTotal_>0){float p=(float)scanDone_/(float)scanTotal_;ImGui::ProgressBar(p,ImVec2(-1,20));ImGui::TextDisabled("%d / %d files",scanDone_.load(),scanTotal_.load());}ImGui::Separator();ImGui::Text("Playback");int threshold=settings_.playbackThreshold();if(ImGui::SliderInt("History threshold (seconds)",&threshold,0,120))settings_.setPlaybackThreshold(threshold);float vol=settings_.defaultVolume();if(ImGui::SliderFloat("Default volume",&vol,0,1))settings_.setDefaultVolume(vol);ImGui::Separator();static char backup[512]="data/music-backup.db";ImGui::InputText("Backup file",backup,sizeof(backup));if(ImGui::Button("Backup Database"))db_.backup(backup);ImGui::SameLine();if(ImGui::Button("Restore Database")){db_.restore(backup);db_.reconcileMissingFiles();}if(ImGui::Button("Reconcile Missing Files"))db_.reconcileMissingFiles();ImGui::Separator();ImGui::Text("Appearance");const char*themes[]={"Dark","Light"};if(ImGui::Combo("Theme",&themeMode_,themes,2)){if(themeMode_==0)ImGui::StyleColorsDark();else ImGui::StyleColorsLight();}if(ImGui::SliderFloat("UI Scale",&uiScale_,0.8f,1.4f,"%.2fx")){ImGui::GetIO().FontGlobalScale=uiScale_;}ImGui::Separator();ImGui::Text("Application");ImGui::TextDisabled("Aurora Player 2.0 • Native C++ • Offline");}
void UI::openMetadata(const Song&s){editingSong_=s;showMetadata_=true;}
void UI::renderMetadataDialog(){if(!showMetadata_)return;ImGui::OpenPopup("Edit Metadata");if(ImGui::BeginPopupModal("Edit Metadata",&showMetadata_,ImGuiWindowFlags_AlwaysAutoResize)){char title[256],artist[256],album[256],aa[256],genre[256];std::snprintf(title,sizeof(title),"%s",editingSong_.title.c_str());std::snprintf(artist,sizeof(artist),"%s",editingSong_.artist.c_str());std::snprintf(album,sizeof(album),"%s",editingSong_.album.c_str());std::snprintf(aa,sizeof(aa),"%s",editingSong_.albumArtist.c_str());std::snprintf(genre,sizeof(genre),"%s",editingSong_.genre.c_str());if(ImGui::InputText("Title",title,sizeof(title)))editingSong_.title=title;if(ImGui::InputText("Artist",artist,sizeof(artist)))editingSong_.artist=artist;if(ImGui::InputText("Album",album,sizeof(album)))editingSong_.album=album;if(ImGui::InputText("Album Artist",aa,sizeof(aa)))editingSong_.albumArtist=aa;if(ImGui::InputText("Genre",genre,sizeof(genre)))editingSong_.genre=genre;ImGui::InputInt("Year",&editingSong_.year);ImGui::InputInt("Track Number",&editingSong_.trackNumber);ImGui::TextDisabled("Changes are stored in Aurora's database and do not modify the audio file.");if(ImGui::Button("Save")){db_.updateSongMetadata(editingSong_);showMetadata_=false;}ImGui::SameLine();if(ImGui::Button("Cancel"))showMetadata_=false;ImGui::EndPopup();}}
// Icons are drawn by hand with ImDrawList instead of relying on Unicode glyphs in the
// loaded font — Windows' base "Segoe UI" doesn't reliably contain codepoints like
// media-transport symbols (⏮⏸⏭) or dingbats (♥), so those were showing up as the
// font's fallback "?" glyph. Hand-drawn shapes render identically regardless of what
// font ends up loaded.
static void drawIcon(ImDrawList*dl,Icon icon,ImVec2 c,float r,ImU32 fg){
    switch(icon){
    case Icon::Play:dl->AddTriangleFilled(ImVec2(c.x-r*0.55f,c.y-r),ImVec2(c.x-r*0.55f,c.y+r),ImVec2(c.x+r*0.85f,c.y),fg);break;
    case Icon::Pause:{float w=r*0.55f;dl->AddRectFilled(ImVec2(c.x-r*0.75f,c.y-r),ImVec2(c.x-r*0.75f+w,c.y+r),fg,2.0f);dl->AddRectFilled(ImVec2(c.x+r*0.75f-w,c.y-r),ImVec2(c.x+r*0.75f,c.y+r),fg,2.0f);break;}
    case Icon::Prev:dl->AddRectFilled(ImVec2(c.x-r,c.y-r),ImVec2(c.x-r+r*0.35f,c.y+r),fg,2.0f),dl->AddTriangleFilled(ImVec2(c.x+r*0.85f,c.y-r),ImVec2(c.x+r*0.85f,c.y+r),ImVec2(c.x-r*0.45f,c.y),fg);break;
    case Icon::Next:dl->AddRectFilled(ImVec2(c.x+r-r*0.35f,c.y-r),ImVec2(c.x+r,c.y+r),fg,2.0f),dl->AddTriangleFilled(ImVec2(c.x-r*0.85f,c.y-r),ImVec2(c.x-r*0.85f,c.y+r),ImVec2(c.x+r*0.45f,c.y),fg);break;
    case Icon::Shuffle:{
        dl->AddLine(ImVec2(c.x-r,c.y-r*0.5f),ImVec2(c.x+r*0.7f,c.y+r*0.5f),fg,2.2f);
        dl->AddLine(ImVec2(c.x-r,c.y+r*0.5f),ImVec2(c.x+r*0.7f,c.y-r*0.5f),fg,2.2f);
        dl->AddTriangleFilled(ImVec2(c.x+r,c.y+r*0.5f),ImVec2(c.x+r*0.45f,c.y+r*0.15f),ImVec2(c.x+r*0.45f,c.y+r*0.85f),fg);
        dl->AddTriangleFilled(ImVec2(c.x+r,c.y-r*0.5f),ImVec2(c.x+r*0.45f,c.y-r*0.15f),ImVec2(c.x+r*0.45f,c.y-r*0.85f),fg);
        break;}
    case Icon::Repeat: case Icon::RepeatOne:{
        dl->PathArcTo(c,r*0.85f,-0.4f,5.6f,20);dl->PathStroke(fg,0,2.2f);
        float ang=5.6f;ImVec2 tip(c.x+r*0.85f*std::cos(ang),c.y+r*0.85f*std::sin(ang));
        ImVec2 dir(std::cos(ang+1.5708f),std::sin(ang+1.5708f));
        dl->AddTriangleFilled(ImVec2(tip.x+dir.x*r*0.55f,tip.y+dir.y*r*0.55f),ImVec2(tip.x-dir.y*r*0.4f,tip.y+dir.x*r*0.4f),ImVec2(tip.x+dir.y*r*0.4f,tip.y-dir.x*r*0.4f),fg);
        if(icon==Icon::RepeatOne){char buf[2]={'1',0};ImVec2 ts=ImGui::CalcTextSize(buf);dl->AddText(ImVec2(c.x-ts.x*0.5f,c.y-ts.y*0.5f),fg,buf);}
        break;}
    case Icon::Queue:for(int i=0;i<3;i++){float y=c.y-r*0.6f+i*(r*0.6f);dl->AddLine(ImVec2(c.x-r,y),ImVec2(c.x+r,y),fg,2.2f);}break;
    case Icon::HeartFilled: case Icon::HeartOutline:{
        float rr=r*0.5f;ImVec2 l(c.x-rr*0.85f,c.y-rr*0.3f),rt(c.x+rr*0.85f,c.y-rr*0.3f);
        if(icon==Icon::HeartFilled){
            dl->AddCircleFilled(l,rr,fg,16);dl->AddCircleFilled(rt,rr,fg,16);
            dl->AddTriangleFilled(ImVec2(c.x-rr*1.7f,c.y-rr*0.15f),ImVec2(c.x+rr*1.7f,c.y-rr*0.15f),ImVec2(c.x,c.y+rr*1.6f),fg);
        }else{
            dl->AddCircle(l,rr,fg,16,1.7f);dl->AddCircle(rt,rr,fg,16,1.7f);
            ImVec2 pts[3]={ImVec2(c.x-rr*1.7f,c.y-rr*0.1f),ImVec2(c.x,c.y+rr*1.6f),ImVec2(c.x+rr*1.7f,c.y-rr*0.1f)};
            dl->AddPolyline(pts,3,fg,0,1.7f);
        }
        break;}
    case Icon::Plus:dl->AddLine(ImVec2(c.x-r,c.y),ImVec2(c.x+r,c.y),fg,2.4f),dl->AddLine(ImVec2(c.x,c.y-r),ImVec2(c.x,c.y+r),fg,2.4f);break;
    }
}
// `id` is the ImGui ID string (e.g. "##prev") and is never shown — only the drawn
// shape is visible, so callers must pass a unique id per button the usual ImGui way.
static bool iconButton(const char*id,Icon icon,bool active,const char*tooltip,float size){
    ImVec2 p=ImGui::GetCursorScreenPos();
    // InvisibleButton's own return value IS the correct "was this clicked" result
    // (press-then-release while hovered, same as a normal Button). An earlier version
    // of this function ignored that and recomputed a "clicked" flag from
    // IsItemClicked() instead, which fires on mouse-DOWN rather than a completed
    // click and doesn't reliably match — that's why the heart/queue/etc buttons
    // weren't registering clicks. Use the real return value.
    bool clicked=ImGui::InvisibleButton(id,ImVec2(size,size));
    bool hovered=ImGui::IsItemHovered(),held=ImGui::IsItemActive();
    ImDrawList*dl=ImGui::GetWindowDrawList();
    ImU32 bg=active?IM_COL32(115,217,166,140):hovered?IM_COL32(255,255,255,26):IM_COL32(255,255,255,0);
    if(held)bg=active?IM_COL32(115,217,166,180):IM_COL32(255,255,255,46);
    if((bg>>24)&0xFF)dl->AddRectFilled(p,ImVec2(p.x+size,p.y+size),bg,6.0f);
    ImU32 fg=active?IM_COL32(120,225,175,255):IM_COL32(228,228,235,255);
    drawIcon(dl,icon,ImVec2(p.x+size*0.5f,p.y+size*0.5f),size*0.28f,fg);
    if(tooltip&&hovered)ImGui::SetTooltip("%s",tooltip);
    return clicked;
}
void UI::renderPlayerBar(){const Song&s=player_.currentSong();if(s.filePath.empty()){ImGui::Dummy(ImVec2(0,(kPlayerBarHeight-ImGui::GetTextLineHeight())*0.5f));ImGui::TextDisabled("Nothing playing");return;}
    // Single row, plain sequential SameLine() calls — same reliable pattern songRow()
    // already uses elsewhere. The previous version split this into two stacked rows
    // inside nested groups with manual cursor Y (and briefly X) repositioning to line
    // the progress bar up under the transport buttons; that hand-rolled math kept
    // producing subtly wrong bounding boxes for the groups after it (which is what
    // was squeezing the volume slider down to almost no usable width). Not worth the
    // fragility for a purely cosmetic two-row look — one row, left to right, is both
    // simpler and correct.
    ImGui::Dummy(ImVec2(0,(kPlayerBarHeight-60.0f)*0.5f-6.0f));
    if(auto*t=textureFor(s.artworkPath)){ImGui::Image((ImTextureID)(intptr_t)t,ImVec2(60,60));ImGui::SameLine();}
    ImGui::BeginGroup();ImGui::Dummy(ImVec2(0,6));ImGui::Text("%s",s.title.c_str());ImGui::TextDisabled("%s",s.artist.c_str());ImGui::EndGroup();
    ImGui::SameLine(230);
    // All transport buttons share one uniform size now — an earlier version made
    // play/pause bigger (38px vs 34px) for emphasis, but since every button shares the
    // same top Y, the bigger one's *center* ends up a couple pixels lower than its
    // neighbors', which read as it being misaligned/shifted down. Same size fixes it.
    constexpr float kIconSize=34.0f;
    if(iconButton("##prev",Icon::Prev,false,"Previous",kIconSize))previous();ImGui::SameLine();
    if(iconButton("##playpause",player_.isPlaying()?Icon::Pause:Icon::Play,false,player_.isPlaying()?"Pause":"Play",kIconSize)){if(player_.isPlaying())player_.pause();else player_.play();}ImGui::SameLine();
    if(iconButton("##next",Icon::Next,false,"Next",kIconSize))advance(true);ImGui::SameLine();
    if(iconButton("##shuffle",Icon::Shuffle,shuffle_,"Shuffle",kIconSize)){shuffle_=!shuffle_;if(shuffle_)rebuildShuffle(db_.songs());}ImGui::SameLine();
    const char*repTip[]={"Repeat off","Repeat all","Repeat one"};
    if(iconButton("##repeat",repeatMode_==2?Icon::RepeatOne:Icon::Repeat,repeatMode_!=0,repTip[repeatMode_],kIconSize))repeatMode_=(repeatMode_+1)%3;
    ImGui::SameLine();
    // Always submit the progress slider, even when duration is momentarily 0 (can
    // happen right as a track starts, or for a file with no duration tag) — skipping
    // the widget entirely on those frames (previous code: `dur>0 && SliderFloat(...)`)
    // meant ImGui's cursor never advanced for it that frame, which shifted everything
    // after it in the row — including the volume slider — sideways unpredictably.
    // Submitting it unconditionally and only *acting* on it when dur>0 keeps the row
    // layout stable every frame regardless.
    float pos=(float)player_.position(),dur=(float)player_.duration();ImGui::SetNextItemWidth(240);if(ImGui::SliderFloat("##progress",&pos,0,dur>0?dur:1.0f,"")&&dur>0)player_.seek(pos);
    ImGui::SameLine();ImGui::TextDisabled("%s / %s",timeFmt(pos).c_str(),timeFmt(dur).c_str());
    ImGui::SameLine();
    // Same widget, same call shape as the working "Default volume" slider in Settings
    // (SliderFloat(label, &v, 0, 1) with no extra format string or flags) — just a
    // hidden label and a smaller width to fit the compact bar. The custom "%.0f%%"
    // format + ImGuiSliderFlags_AlwaysClamp this had before wasn't actually the
    // problem (that turned out to be the click-stealing/layout bugs fixed elsewhere),
    // but matching Settings exactly removes any doubt and is what was asked for.
    float v=player_.volume();ImGui::SetNextItemWidth(140);if(ImGui::SliderFloat("##vol",&v,0,1))player_.setVolume(v);
    ImGui::SameLine();
    if(iconButton("##queue",Icon::Queue,showQueue_,"Queue",kIconSize))showQueue_=!showQueue_;
}
void UI::render(){handlePlaybackProgress();ImGui::SetNextWindowPos(ImVec2(0,0),ImGuiCond_Always);ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize,ImGuiCond_Always);ImGuiWindowFlags flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize;ImGui::Begin("Aurora",nullptr,flags);renderSidebar();ImGui::SameLine();ImGui::BeginChild("content",ImVec2(0,-kPlayerBarHeight),false);switch(page_){case 0:renderHome();break;case 1:renderSearch();break;case 2:renderLibrary();break;case 3:renderPlaylists();break;case 4:renderLiked();break;case 5:renderRecent();break;case 6:renderQueue();break;case 7:renderSettings();break;}ImGui::EndChild();ImGui::BeginChild("playerbar",ImVec2(0,kPlayerBarHeight),true,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);renderPlayerBar();ImGui::EndChild();ImGui::End();if(showQueue_){ImGui::SetNextWindowSize(ImVec2(420,500),ImGuiCond_FirstUseEver);ImGui::Begin("Queue",&showQueue_);renderQueue();ImGui::End();}renderMetadataDialog();}
