#include <SFML/Audio.hpp>
#include <string>
#include <iostream>
#include <vector>

using namespace std;

class Song{
public:
string name;
string artist;
sf::Music music;
string songFile;

void setsonsing(string n, string a ){
    name = n;
    artist = a;
}
void setFile(const string& file) {
        songFile = file;
        if (!music.openFromFile(songFile)) {
            cerr << "Failed to load song: " << songFile << endl;
        }
    }
};
class Playlist {
private:
    string name;
    vector<shared_ptr<Song>> songs;   // shared_ptr if songs can appear in multiple playlists

public:
    Playlist(const string& n) : name(n) {}

    void addSong(shared_ptr<Song> s) {
        songs.push_back(s);
    }

    void removeSong(int index) {
        if (index >= 0 && index < (int)songs.size())
            songs.erase(songs.begin() + index);
    }

    void playAll() {
        for (auto& s : songs) s->play();
    }

    string getName() const { return name; }
    size_t count() const { return songs.size(); }
};

int main()
{
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
