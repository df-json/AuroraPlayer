#include <SFML/Audio.hpp>
#include <string>
#include <iostream>
#include <vector>

using namespace std;

class Song{
public:
string name;
string artist;

string songFile;

void setsonsing(string n, string a ){
    name = n;
    artist = a;
}
void setFile(const string& file) {
        songFile = file;

}
void playSong(const Song& song, sf::Music& music) {
    if (music.openFromFile(song.songFile)) {
        music.play();
    }
}
};
class Playlist {
private:
    string name;
    vector<Song> songs;   // shared_ptr if songs can appear in multiple playlists

public:
    Playlist(const string& n) : name(n) {}

    void addSong(const Song& s) {
        songs.push_back(s);
    }

    void removeSong(int index) {
        if (index >= 0 && index < (int)songs.size())
            songs.erase(songs.begin() + index);
    }

    void playSong(const Song& song, sf::Music& music) {
    if (music.openFromFile(song.songFile)) {
        music.play();
    }
}
    string getName() const { return name; }
    size_t count() const { return songs.size(); }
};

void createplay( vector<Song> &songs){

     int i;
     if (i != 0)

     Playlist playlist(songs[i].name);

}



int main()
{
    sf::Music music;
    vector<Song> songs;
    int i,n;
    string name, artist, file;

    cout<<"How many songs"<<endl;
    cin>>n;
    for (i = 0 ; i < n ; i ++){
        cout<<"Enter Singer and Artist";
        cin>>name;
        cin>>artist;
        Song s;
        s.setsonsing(name,artist);
        cout<<"Enter file"<<endl;
        cin>>file;
        s.setFile(file);
        songs.push_back(move(s));




    }





    return 0;
}
