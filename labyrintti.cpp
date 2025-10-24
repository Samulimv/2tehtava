/* Harjoitus 2: Rinnakkainen labyrinttirotatoteutus */
/* Toimii sekä prosessi- että säietoteutuksella */

/* Peruskirjastot */
#include <iostream>
#include <vector>
#include <string>
#include <cstring>

/* Rinnakkaisuuden kirjastot */
#include <sys/shm.h>   // jaettu muisti
#include <fcntl.h>     // S_IRUSR | S_IWUSR
#include <unistd.h>    // fork(), usleep
#include <sys/wait.h>  // waitpid
#include <pthread.h>

using namespace std;

/* -------------------- Määrittelyt -------------------- */
#define KORKEUS 7
#define LEVEYS 7

// Labyrintti jaettuun muistiin
int (*labyrintti)[LEVEYS] = nullptr;

// Testilabyrintti 7x7
int alkuLabyrintti[KORKEUS][LEVEYS] = {
    {1,1,1,1,1,1,1},
    {1,0,1,0,1,0,4},
    {1,0,1,0,1,0,1},
    {1,2,0,2,0,2,1},
    {1,0,1,0,1,0,1},
    {1,0,1,0,1,0,1},
    {1,1,1,3,1,1,1}
};

/* -------------------- Rakenteet -------------------- */
// Sijainti labyrintissä
struct Sijainti {
    int ykoord {0};
    int xkoord {0};
};

// Suunnat liikkumiseen
enum LiikkumisSuunta { UP, DOWN, LEFT, RIGHT, DEFAULT };

// Risteyksen tutkittu/avoin
enum Ristausve { WALL = 1, OPENING = 0 };

// Suunnan tieto risteyksessä
struct Suunta {
    Ristausve jatkom; 
    bool tutkittu {false};
};

// Risteysrakenne
struct Ristaus {
    Sijainti kartalla;
    Suunta up;
    Suunta down;
    Suunta left;
    Suunta right;
    LiikkumisSuunta tutkittavana = DEFAULT;
};

/* -------------------- Funktiot -------------------- */
// Etsii kohteen labyrintistä
Sijainti etsiKartasta(int kohde){
    Sijainti kartalla;
    for (int y = 0; y < KORKEUS; y++) {
        for (int x = 0; x < LEVEYS; x++){
            if (labyrintti[y][x] == kohde){
                kartalla.xkoord = x;
                kartalla.ykoord = KORKEUS-1-y;
                return kartalla;
            }  
        }
    }
    return kartalla;
}

// Aloituskohta (3)
Sijainti findBegin() {
    return etsiKartasta(3);
}

// Liikkumisfunktiot
Sijainti moveUp(Sijainti s){ s.ykoord++; return s; }
Sijainti moveDown(Sijainti s){ s.ykoord--; return s; }
Sijainti moveLeft(Sijainti s){ s.xkoord--; return s; }
Sijainti moveRight(Sijainti s){ s.xkoord++; return s; }

// Tutkitaan ympäristö
bool tutkiUp(Sijainti nykysijainti, auto& reitti, LiikkumisSuunta prevDir){
    int yindex = KORKEUS-1-nykysijainti.ykoord-1;
    if (yindex < 0) return false;
    if (labyrintti[yindex][nykysijainti.xkoord] == 1) return false;
    if (labyrintti[yindex][nykysijainti.xkoord] == 2 && prevDir != DOWN){
        Ristaus r;
        r.kartalla.ykoord = nykysijainti.ykoord+1;
        r.kartalla.xkoord = nykysijainti.xkoord;
        r.down.tutkittu = true;
        r.down.jatkom = OPENING;
        reitti.push_back(r);
        return true;
    }
    return true;
}

bool tutkiDown(Sijainti nykysijainti, auto& reitti, LiikkumisSuunta prevDir){
    int yindex = KORKEUS-1-nykysijainti.ykoord+1;
    if (yindex >= KORKEUS) return false;
    if (labyrintti[yindex][nykysijainti.xkoord] == 1) return false;
    if (labyrintti[yindex][nykysijainti.xkoord] == 2 && prevDir != UP){
        Ristaus r;
        r.kartalla.ykoord = nykysijainti.ykoord-1;
        r.kartalla.xkoord = nykysijainti.xkoord;
        r.up.tutkittu = true;
        r.up.jatkom = OPENING;
        reitti.push_back(r);
        return true;
    }
    return true;
}

bool tutkiLeft(Sijainti nykysijainti, auto& reitti, LiikkumisSuunta prevDir){
    int xindex = nykysijainti.xkoord-1;
    if (xindex < 0) return false;
    if (labyrintti[KORKEUS-1-nykysijainti.ykoord][xindex] == 1) return false;
    if (labyrintti[KORKEUS-1-nykysijainti.ykoord][xindex] == 2 && prevDir != RIGHT){
        Ristaus r;
        r.kartalla = nykysijainti;
        r.right.tutkittu = true;
        r.right.jatkom = OPENING;
        reitti.push_back(r);
        return true;
    }
    return true;
}

bool tutkiRight(Sijainti nykysijainti, auto& reitti, LiikkumisSuunta prevDir){
    int xindex = nykysijainti.xkoord+1;
    if (xindex >= LEVEYS) return false;
    if (labyrintti[KORKEUS-1-nykysijainti.ykoord][xindex] == 1) return false;
    if (labyrintti[KORKEUS-1-nykysijainti.ykoord][xindex] == 2 && prevDir != LEFT){
        Ristaus r;
        r.kartalla = nykysijainti;
        r.left.tutkittu = true;
        r.left.jatkom = OPENING;
        reitti.push_back(r);
        return true;
    }
    return true;
}

// Etsii seuraavan suunnan
LiikkumisSuunta findNext(bool onkoRistaus, Sijainti nykysij, LiikkumisSuunta prevDir, auto& reitti){
    if (!onkoRistaus){
        if (tutkiLeft(nykysij,reitti,prevDir) && prevDir != RIGHT) return LEFT;
        if (tutkiUp(nykysij,reitti,prevDir) && prevDir != DOWN) return UP;
        if (tutkiDown(nykysij,reitti,prevDir) && prevDir != UP) return DOWN;
        if (tutkiRight(nykysij,reitti,prevDir) && prevDir != LEFT) return RIGHT;
        return DEFAULT;
    } else {
        if (tutkiLeft(nykysij,reitti,prevDir) && reitti.back().tutkittavana != LEFT && !reitti.back().left.tutkittu) return LEFT;
        if (tutkiUp(nykysij,reitti,prevDir) && reitti.back().tutkittavana != UP && !reitti.back().up.tutkittu) return UP;
        if (tutkiDown(nykysij,reitti,prevDir) && reitti.back().tutkittavana != DOWN && !reitti.back().down.tutkittu) return DOWN;
        if (tutkiRight(nykysij,reitti,prevDir) && reitti.back().tutkittavana != RIGHT && !reitti.back().right.tutkittu) return RIGHT;
        return DEFAULT;
    }
}

// Suorittaa risteyksen käsittelyn
LiikkumisSuunta doRistaus(Sijainti risteyssij, LiikkumisSuunta prevDir, auto& reitti){
    LiikkumisSuunta nextDir = findNext(true,risteyssij,prevDir,reitti);
    if (nextDir == DEFAULT) reitti.pop_back();
    else reitti.back().tutkittavana = nextDir;
    return nextDir;
}

// Rotan algoritmi
int aloitaRotta(){
    int liikkuCount = 0;
    vector<Ristaus> reitti;
    Sijainti rotanSij = findBegin();
    LiikkumisSuunta prevDir = DEFAULT;
    LiikkumisSuunta nextDir = DEFAULT;

    while (labyrintti[KORKEUS-1-rotanSij.ykoord][rotanSij.xkoord] != 4){
        if (labyrintti[KORKEUS-1-rotanSij.ykoord][rotanSij.xkoord] == 2){
            nextDir = doRistaus(rotanSij, prevDir, reitti);
        } else nextDir = findNext(false,rotanSij,prevDir,reitti);

        switch(nextDir){
            case UP: rotanSij = moveUp(rotanSij); prevDir = UP; break;
            case DOWN: rotanSij = moveDown(rotanSij); prevDir = DOWN; break;
            case LEFT: rotanSij = moveLeft(rotanSij); prevDir = LEFT; break;
            case RIGHT: rotanSij = moveRight(rotanSij); prevDir = RIGHT; break;
            case DEFAULT:
                if (!reitti.empty()){
                    rotanSij = reitti.back().kartalla;
                    switch(reitti.back().tutkittavana){
                        case UP: reitti.back().up.tutkittu = true; break;
                        case DOWN: reitti.back().down.tutkittu = true; break;
                        case LEFT: reitti.back().left.tutkittu = true; break;
                        case RIGHT: reitti.back().right.tutkittu = true; break;
                        default: break;
                    }
                }
                break;
        }
        liikkuCount++;
        usleep(10);
    }
    return liikkuCount;
}

/* -------------------- Prosessi- ja säietoteutus -------------------- */
int prosessiToteutus(int (*lab)[LEVEYS], int shm_id, int maara){
    vector<pid_t> lapset;
    for (int i=0;i<maara;i++){
        pid_t pid = fork();
        if (pid==0){
            aloitaRotta();
            shmdt(lab);
            return 0;
        } else if (pid > 0){
            lapset.push_back(pid);
        } else {
            perror("fork");
            return 1;
        }
    }
    for (pid_t pid : lapset) waitpid(pid,nullptr,0);
    shmdt(lab);
    shmctl(shm_id,IPC_RMID,nullptr);
    cout << "Kaikki rotat ulkona!" << endl;
    return 0;
}

void* saieToteutus(void*){
    pthread_t id = pthread_self();
    cout << "Säie: " << id << endl;
    aloitaRotta();
    cout << "Säie " << id << " valmis!" << endl;
    return nullptr;
}

/* -------------------- Pääohjelma -------------------- */
int main(){
    int maara;
    string tila;
    cout << "Valitaan ajotapa (p tai s) sekä rottien määrä" << endl;
    cout << "Ajotapa: ";
    cin >> tila;
    if (tila != "p" && tila != "s") { cout << "Väärä syöte!" << endl; return 1; }

    cout << "Rottien määrä: ";
    cin >> maara;

    // Jaettu muisti labyrintille
    size_t jaettuMuisti = sizeof(int)*KORKEUS*LEVEYS;
    int shm_id = shmget(IPC_PRIVATE, jaettuMuisti, S_IRUSR | S_IWUSR);
    labyrintti = (int (*)[LEVEYS]) shmat(shm_id,NULL,0);

    // Kopioidaan testilabyrintti jaettuun muistiin
    memcpy(labyrintti, alkuLabyrintti, sizeof(alkuLabyrintti));

    if (tila == "s"){
        cout << "Ajetaan säikeillä: " << maara << " rottaa!" << endl;
        vector<pthread_t> saikeet(maara);
        for (int i=0;i<maara;i++) pthread_create(&saikeet[i],nullptr,saieToteutus,nullptr);
        for (int i=0;i<maara;i++) pthread_join(saikeet[i],nullptr);
        shmdt(labyrintti);
        shmctl(shm_id,IPC_RMID,nullptr);
    } else {
        cout << "Ajetaan prosesseilla: " << maara << " rottaa!" << endl;
        prosessiToteutus(labyrintti,shm_id,maara);
    }

    cout << "Kaikki rotat ulkona!" << endl;
    return 0;
}
