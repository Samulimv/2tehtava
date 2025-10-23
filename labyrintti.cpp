/* Kääntäminen g++ -w lipulla tällä hetkellä */

/* Harjoituksen 2 ohjelmarunko */
/* Tee runkoon tarvittavat muutokset kommentoiden */
/* Pistesuorituksista kooste tähän alkuun */
/* 2p = täysin tehty suoritus, 1p = osittain tehty suoritus */
/* Tarkemmat ohjeet Moodlessa */
/* Lisäohjeita, vinkkejä ja apuja löytyy koodin joukosta */
/* OPISKELIJA: merkityt kohdat eritoten kannattaa katsoa huolella */

/*-------------/* 
1) Toimiva rinnakkainen prosessitoteutus, jaettua muistia käyttäen, jossa parent odottaa kaikkien lasten pääsemistä suuaukolle. 2p */
/*
2) Toimiva rinnakkainen säietoteutus. 2p */

// peruskirjastot
#include <iostream>
#include <vector>
#include <string>
#include <cstring>

// rinnakkaisuuden peruskirjastot
#include <sys/shm.h> // jaetun muistin kirjasto: shmget(), shmat()
#include <fcntl.h>   // S_IRUSR | S_IWUSR
#include <unistd.h>  // fork()
#include <sys/wait.h>
#include <semaphore.h>
#include <pthread.h>

using namespace std;

//kaikkialla tarvittavat tunnisteet  määritellään globaalilla alueella
//OPISKELIJA: muistele harjoituksista miten eri asiat määriteltiin
//OPISKELIJA: tehtäväsi on sijoittaa itse labyrinttikin jaettuun muistialueeseen ja käyttää sitä sieltä
//->eli:labyrintin käyttö globaalilta alueelta jaetun muistin käyttöön
//vinkki: kannattaa määritellä pointteri nimeltä labyrintti kaksiuloitteiseen taulukkoon jolloin nykytoteutus toimii sellaisenaan
//parent/main alustaa jaettun muistin (eli kirjoittaa sinne) nuo labyrintin alkiot
//poista labyrintti kokonaan globaalilta alueelta, ohjelman pitäisi toimia 
//mieti harjoituksista opitun perusteella paljonko labyrintti vähintään tarvitsee jaettua muistia
//tee kaikki ratkaisut niin että ohjelma toimii millä tahansa labyrintilla

//definet voi jättää globaalille alueelle, ne on sitten tiedossa koko tiedostossa
#define KORKEUS 100
#define LEVEYS 100

int (*labyrintti)[LEVEYS] = nullptr;

//apuja: voit testata ratkaisujasi myös alla olevalla yksinkertaisemmalla labyrintilla 
//#define KORKEUS 7
//#define LEVEYS 7
/*int labyrintti[KORKEUS][LEVEYS] = {
                        {1,1,1,1,1,1,1},
                        {1,0,1,0,1,0,4},
                        {1,0,1,0,1,0,1},
                        {1,2,0,2,0,2,1},
                        {1,0,1,0,1,0,1},
                        {1,0,1,0,1,0,1},
                        {1,1,1,3,1,1,1}};
*/

//karttasijainnin tallettamiseen käytettävä rakenne, luotaessa alustuu vasempaan alakulmaan
//HUOM! ykoordinaatti on peilikuva taulukon rivi-indeksiin
//PasiM: TODO, voisi yksinkertaistaa että ykoord olisi sama kuin rivi-indeksi
struct Sijainti {
    int ykoord {0};
    int xkoord {0};
};

//rotan liikkeen suunnan määrittelyyn käytettävä rakenne
//huom! suunnat ovat absoluuttisia kuin kompassissa
//UP -> ykoord++ (yindex--)
//DOWN -> ykoord-- (yindex++)
//LEFT -> xkoord--
//RIGHT -> xkoord++
//DEFAULT -> suunta tuntematon
enum LiikkumisSuunta {
    UP, DOWN, LEFT, RIGHT, DEFAULT
};
//tämän hetken toteutuksessa tämä on tarpeeton rakenne, voisi käyttää rotan omassa kartan ymmärryksen kasvatuksessa
enum Ristausve {
    WALL = 1,
    OPENING = 0
};

//tämä rakenne on jokaisesta risteyksestä jokaiseen suuntaan omansa
//VINKKI: tutkituksi merkittyyn/merkittävään suuntaan ei rotta koskaan lähde enää tutkimaan ;)
struct Suunta {
    Ristausve jatkom; //tutkituille suunnille tämä on määritelty OPENING arvolle
    bool tutkittu {false}; //alkuarvona tutkimaton
};

//TÄRKEIN kaikista rakenteista, kertoo miten risteys on opittu juuri kyseisen rotan taholta
//tutkittavana - arvo kertoo minne suuntaan juuri tämä rotta viimeksi lähtenyt ko risteyksestä tutkimaan
struct Ristaus {
    Sijainti kartalla;
    Suunta up;
    Suunta down;
    Suunta left;
    Suunta right;
    LiikkumisSuunta tutkittavana = DEFAULT; //alkuarvo, kertoo while-silmukan alussa olevalle risteyskoodille että rotta tuli ensimmäistä kertaa ko risteykseen
};
//poikkeuksenheittämistä varten, ei käytössä tällä hetkellä
//PasiM, TODO: poikkeukset
struct Karttavirhe {
    int koodi {0};
    string msg;
};

//tämä on esittely aloitusalgoritmifunktiolle mitä siis kutsutaan oli kyseessä prosessi eli säietoteutus
//tällä hetkellä palauttaa kyseisen rotan liikkujen määrän labyrintin selvittämiseksi
//OPISKELIJA: yhtenä mielenkiintoisena haasteena voisi olla liikkujen määrän optimointi rottien yhteistyötä kehittämällä
int aloitaRotta();

//OPISKELIJA: lisää tarvittavat muut funktioesittelyt tähän niin koodin järjestykellä tiedostossa ei ole merkitystä


//Rotta-ohjelman algoritmimäärittelyt
//näitä kutsutaan-oli rinnakkaisuus mitä vain
//OPISKELIJA: kun siirrät labyrintin jaettuun muistiin, siihen osoittavaa pointteria varmaankin pitää kuljetella niihin funktioihin missä labyrinttia tutkitaan
//VINKKI: määrittele pointteri niin että olemassa olevissa algoritmeissa oleva koodi toimii sellaisenaan

//etsii kartasta jotain spesifistä, palauttaa sen koordinaatit
Sijainti etsiKartasta(int kohde){
    Sijainti kartalla;
    for (int y = 0; y<KORKEUS ; y++) {
        for (int x = 0; x<LEVEYS ; x++){
            if (labyrintti[y][x] == kohde) {
                kartalla.xkoord = x;
                kartalla.ykoord = KORKEUS-1-y;
                return kartalla;
            }  
        }
    }
    return kartalla;
}

//etsitään labyrintin aloituskohta, merkitty 3:lla
Sijainti findBegin(){
    Sijainti alkusijainti;
    alkusijainti = etsiKartasta(3);
    return alkusijainti;
}

//TÄRKEÄÄ: reitti on risteyspino - sen muutokset täytyy liikkua tiedonvälityksen mukana, siksi liikkuu viittaus siihen
//OPISKELIJA: rotan liikkumislogiikkaan ei (välttämättä) tarvitse kajota, ainoastaan päätöksentekoon risteyksiin liittyen
//OPISKELIJA: väistämislogiikan tekeminen voi olla poikkeus ylläolevaan
//OHJE: taulukon indeksit ja koordinaatit ovat y-suunnassa peilikuvana
//eli: ykoordinaatti alkaa alhaalta kasvaa ylöspäin, yindeksi alkaa ylhäältä kasvaa alaspäin
//OPISKLEIJA: käytä siis labyrinttia jaetusta muistista ja tee tarvittaessa siihen liittyvät muutokset

//tutkitaan mitä nykypaikan yläpuolella on, prevDir kertoo minkä suuntainen oli viimeisin kyseisen rotan liikku
bool tutkiUp(Sijainti nykysijainti, auto& reitti, LiikkumisSuunta prevDir){
    int yindex = KORKEUS-1-nykysijainti.ykoord-1;
    if (yindex < 0) return false; 
    if (labyrintti[yindex][nykysijainti.xkoord] == 1) return false; 
    if (labyrintti[yindex][nykysijainti.xkoord] == 2 && prevDir != DOWN) {
        Ristaus ristaus;
        ristaus.kartalla.ykoord = nykysijainti.ykoord+1;
        ristaus.kartalla.xkoord = nykysijainti.xkoord;
        ristaus.down.tutkittu = true;
        ristaus.down.jatkom = OPENING;
        reitti.push_back(ristaus);
        return true;
    }
    return true;
}
//alas
bool tutkiDown(Sijainti nykysijainti, auto& reitti, LiikkumisSuunta prevDir){
    int yindex = KORKEUS-1-nykysijainti.ykoord+1;
    if (yindex >= KORKEUS) return false; 
    if (labyrintti[yindex][nykysijainti.xkoord] == 1) return false;
    if (labyrintti[yindex][nykysijainti.xkoord] == 2 && prevDir != UP){
        Ristaus ristaus;
        ristaus.kartalla.ykoord = nykysijainti.ykoord-1;
        ristaus.kartalla.xkoord = nykysijainti.xkoord;
        ristaus.up.tutkittu = true;
        ristaus.up.jatkom = OPENING;
        reitti.push_back(ristaus);
        return true;
    }
    return true;
}
//vasen
bool tutkiLeft(Sijainti nykysijainti, auto& reitti, LiikkumisSuunta prevDir){
    int yindex = KORKEUS-1-nykysijainti.ykoord;
    int xindex = nykysijainti.xkoord-1;
    if (xindex < 0) return false; 
    if (labyrintti[yindex][xindex] == 1) return false;
    if (labyrintti[yindex][xindex] == 2 && prevDir != RIGHT){
        Ristaus ristaus;
        ristaus.kartalla.ykoord = nykysijainti.ykoord;
        ristaus.kartalla.xkoord = nykysijainti.xkoord-1;
        ristaus.right.tutkittu = true;
        ristaus.right.jatkom = OPENING;
        reitti.push_back(ristaus);
        return true;
    }
    return true;
}
//oikea
bool tutkiRight(Sijainti nykysijainti, auto& reitti, LiikkumisSuunta prevDir){
    int yindex = KORKEUS-1-nykysijainti.ykoord;
    int xindex = nykysijainti.xkoord+1;
    if (xindex >= LEVEYS) return false; 
    if (labyrintti[yindex][xindex] == 1) return false;
    if (labyrintti[yindex][xindex] == 2 && prevDir != LEFT){
        Ristaus ristaus;
        ristaus.kartalla.ykoord = nykysijainti.ykoord;
        ristaus.kartalla.xkoord = nykysijainti.xkoord+1;
        ristaus.left.tutkittu = true;
        ristaus.left.jatkom = OPENING;
        reitti.push_back(ristaus);
        return true;
    }
    return true;
}

//tämä funktio palauttaa aina seuraavan lähtösuunnan ilman lisäehtoja
//OPISKELIJA: älä muuta tätä funktiota suoraan vaan tee omille mahdollisille lisäehdoille oma(t) funktio(t) loogisesti oikeisiin paikkoihin
//muutettu
LiikkumisSuunta findNext(bool onkoRistaus, Sijainti nykysijainti, LiikkumisSuunta prevDir, auto& reitti){
    if (!onkoRistaus) {        
        if (tutkiLeft(nykysijainti, reitti, prevDir) && prevDir != RIGHT) return LEFT;
        if (tutkiUp(nykysijainti, reitti, prevDir) && prevDir != DOWN) return UP;
        if (tutkiDown(nykysijainti, reitti, prevDir) && prevDir != UP) return DOWN;
        if (tutkiRight(nykysijainti, reitti, prevDir) && prevDir != LEFT) return RIGHT;
        return DEFAULT;
    }
    else if (onkoRistaus) {
        if (tutkiLeft(nykysijainti, reitti, prevDir) && reitti.back().tutkittavana != LEFT && !reitti.back().left.tutkittu) return LEFT;
        if (tutkiUp(nykysijainti, reitti, prevDir) && reitti.back().tutkittavana != UP && !reitti.back().up.tutkittu) return UP;
        if (tutkiDown(nykysijainti, reitti, prevDir) && reitti.back().tutkittavana != DOWN && !reitti.back().down.tutkittu) return DOWN;
        if (tutkiRight(nykysijainti, reitti, prevDir) && reitti.back().tutkittavana != RIGHT && !reitti.back().right.tutkittu) return RIGHT;
        return DEFAULT;
    }
}

Sijainti moveUp(Sijainti nykysijainti){ nykysijainti.ykoord++; return nykysijainti;}
Sijainti moveDown(Sijainti nykysijainti){ nykysijainti.ykoord--; return nykysijainti;}
Sijainti moveLeft(Sijainti nykysijainti){ nykysijainti.xkoord--; return nykysijainti;}
Sijainti moveRight(Sijainti nykysijainti){ nykysijainti.xkoord++; return nykysijainti;}

LiikkumisSuunta doRistaus(Sijainti risteyssijainti, LiikkumisSuunta prevDir, auto& reitti){
    LiikkumisSuunta nextDir; 
    nextDir = findNext(true, risteyssijainti, prevDir, reitti); 
    if (nextDir == LEFT) reitti.back().tutkittavana = LEFT;
    else if (nextDir == UP) reitti.back().tutkittavana = UP;
    else if (nextDir == RIGHT) reitti.back().tutkittavana = RIGHT;
    else if (nextDir == DOWN) reitti.back().tutkittavana = DOWN;
    else if (nextDir == DEFAULT) reitti.pop_back();
    return nextDir;
}

int aloitaRotta(){
    int liikkuCount=0;
    vector<Ristaus> reitti;
    Sijainti rotanSijainti = findBegin();
    LiikkumisSuunta prevDir {DEFAULT};
    LiikkumisSuunta nextDir {DEFAULT};
    while (labyrintti[KORKEUS-1-rotanSijainti.ykoord][rotanSijainti.xkoord] != 4) {
        if (labyrintti[KORKEUS-1-rotanSijainti.ykoord][rotanSijainti.xkoord] == 2){
            nextDir = doRistaus(rotanSijainti, prevDir, reitti);
        }
        else nextDir = findNext(false, rotanSijainti, prevDir, reitti);
        switch (nextDir) {
        case UP: rotanSijainti = moveUp(rotanSijainti); prevDir = UP; break;
        case DOWN: rotanSijainti = moveDown(rotanSijainti); prevDir = DOWN; break;
        case LEFT: rotanSijainti = moveLeft(rotanSijainti); prevDir = LEFT; break;
        case RIGHT: rotanSijainti = moveRight(rotanSijainti); prevDir = RIGHT; break;
        case DEFAULT:
            if (!reitti.empty()) {
                rotanSijainti.ykoord = reitti.back().kartalla.ykoord;
                rotanSijainti.xkoord = reitti.back().kartalla.xkoord;
                switch (reitti.back().tutkittavana){
                    case UP: reitti.back().up.tutkittu = true; reitti.back().up.jatkom = OPENING; break;
                    case DOWN: reitti.back().down.tutkittu = true; reitti.back().down.jatkom = OPENING; break;
                    case LEFT: reitti.back().left.tutkittu = true; reitti.back().left.jatkom = OPENING; break;
                    case RIGHT: reitti.back().right.tutkittu = true; reitti.back().right.jatkom = OPENING; break;
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

/*------Prosessitoteutus-----*/
int prosessiToteutus(int (*labyrintti)[LEVEYS], int shm_id, int maara) {
     vector<pid_t> lapset;
     for (int i = 0; i < maara; i++) {
         pid_t pid = fork();
         if (pid == -1) { perror("fork"); return 1; };
         if (pid == 0) {
             aloitaRotta();
             shmdt(labyrintti);
             return 0;
         } else {
             lapset.push_back(pid);
         }
     }
    for (pid_t pid : lapset) waitpid(pid, nullptr, 0);
    shmdt(labyrintti);
    shmctl(shm_id, IPC_RMID, nullptr);
    cout << "Kaikki rotat ulkona!" << endl;
    return 0;
}

/*------Säie toteutus-------*/
void* säieToteutus(void* arg) {
    pthread_t id = pthread_self();
    cout << "Säie: " << id << endl;
    aloitaRotta();
    cout << "Säie " << id << " valmis!" << endl;
    return nullptr;
}

//Pääohjelma
int main(int argc, char* argv[]){ 
    int maara;
    string tila;
    cout << "Valitaan ajotapa (p tai s) sekä rottien määrä" << endl;
    cout << "Ajotapa: ";
    cin >> tila;
    if (tila == "p" || tila == "s") {
        cout << "Rottien määrä: ";
        cin >> maara;
    }

    size_t jaettuMuisti = sizeof(int) * KORKEUS * LEVEYS;
    int shm_id;
    shm_id = shmget(IPC_PRIVATE, jaettuMuisti, S_IRUSR | S_IWUSR);
    labyrintti = (int (*)[LEVEYS]) shmat(shm_id, NULL, 0);

    int alkupLabyrintti[KORKEUS][LEVEYS] = {0};
    memcpy(labyrintti, alkupLabyrintti, jaettuMuisti);

    if (tila == "s") {
        cout << "Ajetaan säikeillä: " << maara << " rottaa!" << endl;
        vector<pthread_t> säikeet(maara);
        for (int i = 0; i < maara; i++) pthread_create(&säikeet[i], nullptr, säieToteutus, labyrintti);
        for (int i = 0; i < maara; i++) pthread_join(säikeet[i], nullptr);
        shmdt(labyrintti);
        shmctl(shm_id, IPC_RMID, nullptr);
        cout << "Kaikki rotat ulkona!" << endl;
    } else if (tila == "p") {
        cout << "Ajetaan prosesseilla: " << maara << " rottaa!" << endl;
        prosessiToteutus(labyrintti, shm_id, maara);
    } else {
        cout << "Väärä syöte: " << tila << endl;
        return 1;
    }

    std::cout << "Kaikki rotat ulkona!" << endl;
    return 0;
}
