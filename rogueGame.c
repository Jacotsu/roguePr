#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>

//grafica terminale
#include <unistd.h>
#include <termios.h>

//audio
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <fcntl.h>

//utility di debug
#define DEBUG 0 

#ifdef DEBUG
#define DEBUG_TEST 1
#else
#define DEBUG_TEST 0
#endif

#define debug_print(fmt, ...) \
	        do { \
			if (DEBUG) \
				fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__,\
				       __LINE__, __func__,##__VA_ARGS__);\
	       	} while (0);
//---------------------------------------------------------------

#define max_block_types 7
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

enum mapBlocks {vuoto, muro, spawnGiocatore, end, erba, monster,playerSpr};
enum direzioni {dx,sx,su,giu};
enum factions {playerFac, allies, enemies};


struct voceMenu;
struct menu;
struct canvas;
struct gameSession;

char *mappa_blocchi[max_block_types] = {"\u2591","\u2588","\x1b[36m\u2698\x1b[0m","\x1b[32m\u2591\x1b[0m","\x1b[32m\u03e1\x1b[0m", "\x1b[31m\u057b\x1b[0m", "\x1b[36m\u2698\x1b[0m"};
char *mappa_direzioni[4] = {"destra","sinistra","su", "giu"};
char *factionNames[] = {"Player","Alleato","Nemico"};

#define MAX_EVENTS 48
struct eventConsole {
	char **buffer;
	int lastEvent,maxEvents;
};

struct pos {
	int x,y;
};
typedef struct pos size;


struct canvas {
	size dim;
	struct pos cursore;
	char **data;
};

struct map {
	int len;
	enum mapBlocks *data;
};

struct voceMenu {
	char *testo;
	struct menu *subMenu; //puntatore ad un sottomenu, se NULL viene ignorato
	int (*action)(); //puntatore alla funzione da eseguire, se NULL viene ignorato, ritorna 1 se deve uscire dal menu
};

struct menu {
	int len;
	struct voceMenu *vociMenu;
};

struct viewport {
	struct pos *posizione;
	struct pos *relToCanvas;
	size dimensioni;
};

#define MAX_HP 100
#define MAX_STR 50
struct entity {
	struct pos *posizione;
	float hp,str;
	enum factions faction;
	char *img;
	void (*onUpdate)(struct gameSession*, struct entity*);//chiamata quando l'entità si aggiorna
	void (*onDeath)(struct gameSession*, struct entity*);//chiamata quando l'entità si aggiorna
};

#define MAX_ENTITIES 250
struct entityPool {
	int maxEntities, last;
	struct entity *entities[MAX_ENTITIES];
};

struct audioStream {
	FILE *file;
	char *filename;
};

enum gameStates {finished,running};

struct gameSession {
	struct map *mappa;
	struct viewport *viewport;
	struct canvas *canvas;
	struct entityPool *poolEntita;
	struct viewport *consoleViewport;
	struct eventConsole *eventConsole;
	enum gameStates state;
};


//map handling functions
struct map *loadMap(char*);
void printMap(struct map*,struct viewport *);
void testBlocks();
struct map *genMap(int);
void freeMap(struct map*);
void digMap(struct map *, struct pos ,int *);
void saveMap(char *,struct map *);
int askMapSize();
void swapMapBlocks(struct map*,struct pos, struct pos);
void printEntireMap(struct map*);
struct pos *rndMapSpot(struct map *, enum mapBlocks);
//	void printEntities()

//core
void play(struct gameSession *);
struct gameSession* initGameSession(struct map*, struct viewport*, struct viewport*, size);

//game mechanics
int moveEntity(struct gameSession *,struct entity *,enum direzioni);
struct pos *getPlayerSpawn(struct map *);
struct pos *getGoalPos(struct map *);
struct entity *genRndMonster(struct gameSession*);
int fight(struct gameSession*, struct entity*, struct entity*);
void regenEntity(struct gameSession*, struct entity *);
void execAI(struct gameSession*, struct entity *);
void playerDeath(struct gameSession *, struct entity *);
void monsterDeath(struct gameSession *,struct entity *);


//entity management
struct entity *spawnEntity(struct gameSession*,struct pos*,char *,enum factions, void (*)(struct gameSession*,struct entity*),void (*)(struct gameSession*,struct entity*));
int manageEntityMove(struct gameSession*,struct entity*, struct pos*);
struct entityPool *initEntityPool();
void registerEntity(struct entityPool*, struct entity*);
void freeEntityPool(struct entityPool*);
void updateEntityPool(struct gameSession*);
struct entity *getEntityAt(struct gameSession*,struct pos *);
void killEntity(struct gameSession *,struct entity*);

//util functions
void shuffleArray(int *, int );
char getch();
void cls();

//menu handling functions
void manMenu(struct menu *);
struct menu *initMenu();
void printMenu(struct menu *);
void freeMenu(struct menu *);

//menu functions
int genAndPlay();
int genAndSave();
int loadAndPlay();
int esci();

//audio
struct audioStream *playAudio(char *);
void stopAudio(struct audioStream *);
int isAudioStreamPlaying(struct audioStream *);

//event console

struct eventConsole *initEventConsole();
void printEventConsole(struct eventConsole*);
void printStrEventConsole(struct eventConsole*,char *);
void freeEventConsole(struct eventConsole*);

//canvas
struct canvas *initCanvas(size);
void freeCanvas(struct canvas*);

//rendering
void renderMap(struct gameSession*);
void renderEntities(struct gameSession*);
void renderCanvas(struct canvas *);
void renderEventConsole(struct gameSession *session);

int main() {
	struct map *mappa;
	srand(time(NULL));

	if (DEBUG) {
		struct viewport pov;
		pov.posizione = malloc(sizeof(struct pos));

		testBlocks();
		mappa = loadMap("map1.txt");
		printEntireMap(mappa);

		genMap(25);

		mappa = loadMap("map2.txt");
		printEntireMap(mappa);

		mappa = loadMap("gameOver.map");
		printEntireMap(mappa);

		mappa = loadMap("win.map");
		printEntireMap(mappa);

	};

	struct menu *mainMenu = initMenu();
	manMenu(mainMenu);
	printf("%s",ANSI_COLOR_RESET);
	return 0;
}

//session management ------------------------------------------------
struct gameSession* initGameSession(struct map *mappa, struct viewport *viewport, struct viewport *consoleViewport, size dim) {
	struct gameSession *session = malloc(sizeof(struct gameSession));
	session->mappa = mappa;
	session->viewport = viewport;
	session->canvas = initCanvas(dim);
	session->poolEntita = initEntityPool();
	session->eventConsole = initEventConsole();
	session->consoleViewport = consoleViewport;
	session->state = running;

	if (DEBUG) {
		printStrEventConsole(session->eventConsole,"test");
		printStrEventConsole(session->eventConsole,"test1");
		printStrEventConsole(session->eventConsole,"test2");
		printStrEventConsole(session->eventConsole,"test3");
		printStrEventConsole(session->eventConsole,"test4");
		printStrEventConsole(session->eventConsole,"testOverflow");
	}
	return session;
};

void freeGameSession(struct gameSession *session) {
//	freeCanvas(session->canvas);
	free(session->eventConsole);
	freeEntityPool(session->poolEntita);
	free(session);
}
//------------------------------------------------

//canvas management-----------------------------------------------------------
struct canvas *initCanvas(size dim) {
	debug_print("Canvas inizializzata %dx%d\n",dim.x,dim.y);
	struct canvas *canvas = malloc(sizeof(struct canvas));
	canvas->dim = dim;
	canvas->cursore.x = 0;
	canvas->cursore.y = 0;
	canvas->data = malloc(sizeof(char*)*dim.x*dim.y);
	for(int i = 0; i < dim.x*dim.y; ++i) canvas->data[i] = "\0";
	return canvas;
};

void freeCanvas(struct canvas *canvas) {
	free(canvas->data); //per qualche motivo va in segfault
	free(canvas);
};

//---------------------------------------------------------------------------------

//entity management --------------------------------------------
struct entityPool *initEntityPool() {
	struct entityPool *pool = malloc(sizeof(struct entityPool));
	pool->maxEntities = MAX_ENTITIES;
	pool->last = 0;
	memset(pool->entities,0,MAX_ENTITIES*sizeof(struct entity*));
	return pool;
};

void updateEntityPool(struct gameSession *session) {
	for (int i = 0; i < session->poolEntita->last; ++i) {
		if (session->poolEntita->entities[i]) {
			if (session->poolEntita->entities[i]->onUpdate) session->poolEntita->entities[i]->onUpdate(session,session->poolEntita->entities[i]);
		}
	}
}

void registerEntity(struct entityPool *pool, struct entity *entita) {
	pool->entities[pool->last++] = entita;
};

void unregisterEntity(struct entityPool *pool,struct entity *ent) {
	for (int i = 0; i < pool->last; ++i) {
		if (pool->entities[i] == ent) {
			free(ent->posizione);
			free(pool->entities[i]);
			pool->entities[i] = pool->entities[--pool->last];
		}
	};
}

void freeEntityPool(struct entityPool *pool) {
	for (int i = 0; i < pool->last; ++i)
		if (pool->entities[i]) {
			free(pool->entities[i]->posizione);
			free(pool->entities[i]);
		}
	free(pool);
}

void killEntity(struct gameSession *session,struct entity *entity) {
	entity->onDeath(session,entity);
};

struct entity *spawnEntity(struct gameSession *session, struct pos *posizione,char *sprite,enum factions fazione,void (*updateFunc)(struct gameSession*,struct entity*),void (*deathFunc)(struct gameSession*,struct entity*)) {
	struct entity *tmp = malloc(sizeof(struct entity));
	tmp->hp = MAX_HP;
	tmp->str = MAX_STR;
	tmp->faction = fazione;
	tmp->posizione = posizione;
	tmp->img = sprite;
	tmp->onUpdate = updateFunc;
	tmp->onDeath = deathFunc;
	debug_print("Spawned entity {HP:%.0f STR:%.0f FAC:%d X:%d Y:%d}\n",tmp->hp,tmp->str,tmp->faction,tmp->posizione->x,tmp->posizione->y);

	registerEntity(session->poolEntita,tmp);
	return tmp;
};

struct entity *getEntityAt(struct gameSession *session, struct pos *posizione) {
	for (int i = 0; i < session->poolEntita->maxEntities ; ++i)
		if (session->poolEntita->entities[i] && session->poolEntita->entities[i]->posizione->x == posizione->x && session->poolEntita->entities[i]->posizione->y == posizione->y) {
			debug_print("Entita trovata in (%d,%d)\n",posizione->x,posizione->y);
			return session->poolEntita->entities[i];
		}
	debug_print("Entita non trovata\n");
	return NULL;
};

void regenEntity(struct gameSession *session, struct entity *entity) {
	const float rate = 0.005;
	if (entity->hp < MAX_HP) entity->hp += rate;
       	if (entity->hp > MAX_HP) entity->hp = MAX_HP;
};

void monsterDeath(struct gameSession *session,struct entity *entity) {
	unregisterEntity(session->poolEntita,entity);
}

void playerDeath(struct gameSession *session, struct entity *entity) {
	//gameover
	struct audioStream *defeatMusic = playAudio("bgm.waw");//trick per stoppare la bgm senza una referenza diretta ad essa
	stopAudio(defeatMusic);

	playAudio("Defeat.waw");
	struct map *map = loadMap("gameOver.map");
	printEntireMap(map);
	printf("Premi q per continuare: \n");
	char tmp = getch();
	stopAudio(defeatMusic);
	session->state = finished;
};

void execAI(struct gameSession *session,struct entity *entity) {
	regenEntity(session,entity);
//movimento randomico
	int directions[4] = {dx,sx,su,giu};
	shuffleArray(directions,4);

	moveEntity(session,entity,directions[0]);
};

//return -1 if failed otherwise the block type the player is standing on
int moveEntity(struct gameSession *session,struct entity *entity,enum direzioni direzione) {
	struct pos posizione;
	posizione.x = entity->posizione->x;
	posizione.y = entity->posizione->y;

	switch(direzione) {
		case su:{
			--posizione.y;
			if(manageEntityMove(session,entity,&posizione)) {
				--entity->posizione->y;
			} else return -1;
			break;
			}
		case giu:{
			++posizione.y;
			if(manageEntityMove(session,entity,&posizione)) {
			 	  ++entity->posizione->y;
			   }   else return -1;
			break;
			 }
		case dx: {
			++posizione.x;
			if(manageEntityMove(session,entity,&posizione)) {
				 ++entity->posizione->x;
			 } else return -1;
			break;
			 }
		case sx: {
			--posizione.x;
			if(manageEntityMove(session,entity,&posizione)) {
				 --entity->posizione->x;
			  } else return -1;
			break;
			 }
	};
	debug_print("Movimento entita da (%d;%d) verso %s\n",entity->posizione->x, entity->posizione->y,mappa_direzioni[direzione]);
	return session->mappa->data[session->mappa->len*entity->posizione->y + entity->posizione->x];
};

int manageEntityMove(struct gameSession *session, struct entity *entity, struct pos *posizione) {
	enum mapBlocks bloccoDest = session->mappa->data[session->mappa->len * posizione->y + posizione->x];
	switch (bloccoDest) {
		case vuoto: {
				struct entity *nemico = getEntityAt(session,posizione);
			    	if (nemico && nemico->faction != entity->faction) {
					fight(session,entity,nemico);
					return 0;
				};
			    }
				return 1;
			    break;
		case end: return 1;
			  break;
		case spawnGiocatore: return 1;
				     break;
		default: return 0;
	}

};

int fight(struct gameSession *session, struct entity* a, struct entity* b) {
	int attacker = rand()%2;
	debug_print("--------Inizio combattimento--------\n");
	if (attacker) {
		float dmg = (rand()%101/100.0)*a->str;
		debug_print("A attacca B con danno: %.0f HP\n",dmg);

		b->hp -= dmg;
		char msg[100];
		sprintf(msg,"%s attacca ed infligge %.0f HP di danno, HP rimanenti a %s %.0f",factionNames[a->faction],dmg,factionNames[b->faction],b->hp);
		printStrEventConsole(session->eventConsole,msg);

		if (b->hp <= 0) {
			if (b->faction == playerFac) printStrEventConsole(session->eventConsole,"Sconfitta");
			else printStrEventConsole(session->eventConsole,"Vittoria");

			killEntity(session,b);
			debug_print("---------Fine combattimento--------\n");
			return 1;
		}
		attacker = 0;
	} else {
		float dmg = (rand()%101/100.0)*b->str;
		debug_print("B attacca A con danno: %.0f HP\n",dmg);

		a->hp -= dmg;
		char msg[100];
		sprintf(msg,"%s attacca ed infligge %.0f HP di danno, HP rimanenti a %s %.0f",factionNames[a->faction],dmg,factionNames[b->faction],b->hp);
		printStrEventConsole(session->eventConsole,msg);

		if (a->hp <= 0) {
			if (a->faction == playerFac) printStrEventConsole(session->eventConsole,"Sconfitta");
			else printStrEventConsole(session->eventConsole,"Vittoria");

			killEntity(session,a);
			debug_print("---------Fine combattimento--------\n");
			return 0;
		}
		attacker = 1;
	};
};

struct entity *genRndMonster(struct gameSession *session) {
	const int fullHp = 100;
	const int maxStr = 10;

	struct entity *mon = malloc(sizeof(struct entity));
	mon->posizione = rndMapSpot(session->mappa,vuoto);
	mon->hp = fullHp;
	mon->str = maxStr;
	mon->faction = enemies;
	mon->img = mappa_blocchi[monster];
	mon->onUpdate = execAI;
	mon->onDeath = monsterDeath;
	registerEntity(session->poolEntita,mon);
	return mon;
};

//----------------------------------------------------------------



//event console---------------------------------------------------------
struct eventConsole *initEventConsole() {
	struct eventConsole *console = malloc(sizeof(struct eventConsole));
	console->buffer = malloc(sizeof(char*)*MAX_EVENTS);
	for (int i = 0; i < MAX_EVENTS; ++i) {
		console->buffer[i] = malloc(sizeof(char)*100);
		console->buffer[i][0] = '\0';
	}
	console->lastEvent = 0;
	console->maxEvents = MAX_EVENTS;
};

void printEventConsole(struct eventConsole *console) {
	int tmp = console->lastEvent;

	for (int i = 0; i < MAX_EVENTS; ++i) {
		printf("%s\n",console->buffer[((tmp+i)%MAX_EVENTS)]);
	};
};

void printStrEventConsole(struct eventConsole *console, char *str){
	char tmp[100];
	sprintf(tmp,"  %s",str);
	strcpy(console->buffer[(console->lastEvent++ % MAX_EVENTS)],tmp);
};

void freeEventConsole(struct eventConsole *console) {
	for (int i = 0; i < MAX_EVENTS; ++i) free(console->buffer[i]);
	free(console->buffer);
	free(console);
};
//----------------------------------------------------------------------

//audio-------------------------------------------------------------
struct audioStream *playAudio(char *filename) {
	struct audioStream *stream = malloc(sizeof(struct audioStream));
	char cmd[256];
	debug_print("Riproduzione file audio: %s\n",filename);
	strcpy(cmd,"aplay -q sounds/");
	strcat(cmd,filename);
	FILE* file = popen(cmd, "r");
	stream->filename = filename;
	stream->file = file;
    	return stream;
};

int isAudioStreamPlaying(struct audioStream *stream) {
	char cmd[256];
	strcpy(cmd,"pgrep -fl \"^(sh -c) aplay -q sounds/");//pgrep ritorna 0 se 1 o più processi matchano
	strcat(cmd,stream->filename);
	strcat(cmd,"\"");
	int ris = system(cmd);
	debug_print("Brano in riproduzione?: %s (%s) %d\n",stream->filename,cmd,ris);
	return WEXITSTATUS(ris) == 0;
};

void stopAudio(struct audioStream *stream) {
	char cmd[256];
	strcpy(cmd, "pkill -9 -f \"aplay -q sounds/");
	strcat(cmd,stream->filename);
	strcat(cmd,"\"");
	debug_print("Stopping audio: %s\n",cmd);
	system(cmd);//killa il processo audio
	pclose(stream->file);
	free(stream);
};
//--------------------------------------------------------------------

//auxiliary functions ------------------------------------------------
char getch(void) {
      struct termios org_opts, new_opts;
      int res=0;
      int c=0;
      //-----  store old settings -----------
      res=tcgetattr(STDIN_FILENO, &org_opts);
      //---- set new terminal parms --------
      memcpy(&new_opts, &org_opts, sizeof(new_opts));
      new_opts.c_lflag &= ~(ICANON | ECHO);
      tcsetattr(STDIN_FILENO, TCSANOW, &new_opts);
      do {
		read(0,&c,1);
      } while (c < 32 || c > 127);
      debug_print("Pressed: %c (%d)\n",c,c);
      //------  restore old settings ---------
      res=tcsetattr(STDIN_FILENO, TCSANOW, &org_opts);
      return c;
};

int genAndPlay(){
	printf(".:Generatore mappe:.\n");


	struct pos relToCanvasConsole = {91,0};
	struct viewport povConsole;
	povConsole.relToCanvas = &relToCanvasConsole;

	struct pos relToCanvas = {0,0};//posizione di rendering relativo allo schermo
	struct viewport pov;
	pov.dimensioni.x = 90;
	pov.dimensioni.y = 48;

	size dim = {180,48};//dimensioni di schermo stampabile

	struct gameSession *sessione = initGameSession(genMap(askMapSize()),&pov,&povConsole,dim);
	play(sessione);

	freeMap(sessione->mappa);
	freeGameSession(sessione);
	return 0;
};

int genAndSave(){
	printf(".:Generatore mappe:.\n");
	struct map *mappa;
	char name[256];

	mappa = genMap(askMapSize());

	printEntireMap(mappa);
	printf("Inserisci il nome della mappa (max 256 chars): ");
	scanf("%s",name);
	saveMap(name,mappa);

	return 0;
};

int loadAndPlay(){
	printf(".:Carica mappa e gioca:.\n");

	char name[256];
	struct map *map;
	do {
		printf("Inserisci il nome della mappa (max 256 chars): ");
		scanf("%s",name);
		map = loadMap(name);
	} while (!map);

	struct pos relToCanvasConsole = {91,0};
	struct viewport povConsole;
	povConsole.relToCanvas = &relToCanvasConsole;

	struct pos relToCanvas = {0,0};//posizione di rendering relativo allo schermo
	struct viewport pov;
	pov.dimensioni.x = 90;
	pov.dimensioni.y = 48;

	size dim = {180,48};//dimensioni di schermo stampabile

	struct gameSession *sessione = initGameSession(map,&pov,&povConsole,dim);
	play(sessione);

	freeMap(sessione->mappa);
	freeGameSession(sessione);
	return 0;
};

int esci(){
	return 1;
};

void testBlocks() {
	for (int i = 0; i < max_block_types; ++i) debug_print("%d:%s ",i,mappa_blocchi[i]);
	debug_print("%s\n","");
}

void cls() {
	printf("\e[1;1H\e[2J"); // POSIX clear screen ansi escape
};

void shuffleArray(int *array, int size) {
	for (int i = 0; i < size; ++i) {
		int swap = array[i];
		int offset = rand()%(size-i);
		array[i] = array[i+offset];
		array[i+offset] = swap;
	}
}

int askMapSize() {
	int size = 0;
	while (1) {
		printf("Inserisci le dimensioni della mappa (devono essere dispari): ");
		scanf("%d",&size);
		if ((size % 2 == 0) || (size < 0)) printf("Dimensioni invalide: %d\n",size);
		else return size;
	};
}

//--------------------------------------------------------------------


//core play --------------------------------------------------------
struct pos *getPlayerSpawn(struct map *mappa){
	for (int y = 0; y < mappa->len; ++y)
		for (int x = 0; x < mappa->len; ++x)
			if (mappa->data[y*mappa->len + x] == spawnGiocatore) {
				struct pos *tmp = malloc(sizeof(struct pos));
				tmp->x = x;
				tmp->y = y;
				return tmp;
			}
	return NULL;
};

struct pos *getGoalPos(struct map *mappa) {
	for (int y = 0; y < mappa->len; ++y)
		for (int x = 0; x < mappa->len; ++x)
			if (mappa->data[y*mappa->len + x] == end) {
				struct pos *tmp = malloc(sizeof(struct pos));
				tmp->x = x;
				tmp->y = y;
				return tmp;
			}
	return NULL;

};

void play(struct gameSession *session) {
	struct pos *playerPos = getPlayerSpawn(session->mappa);
	struct entity *player = spawnEntity(session,playerPos,mappa_blocchi[playerSpr],playerFac,regenEntity,playerDeath);
	session->viewport->posizione = playerPos;
	struct pos *goalPos = getGoalPos(session->mappa);

	debug_print("player(%d;%d) exit(%d;%d)\n",playerPos->x,playerPos->y,goalPos->x,goalPos->y);

	for (int i = 0; i < (rand()+1)%(session->mappa->len-4);++i) genRndMonster(session);

	struct audioStream *bgmMusic = playAudio("bgm.waw");
	do {
		char key;
		if (!isAudioStreamPlaying(bgmMusic)) {
			stopAudio(bgmMusic);
			bgmMusic = playAudio("bgm.waw");
		};

		cls();
		updateEntityPool(session);
		renderMap(session);
		renderEntities(session);
		renderEventConsole(session);
		renderCanvas(session->canvas);
		printf("Usa WASD per muoverti, q per uscire: ");
		fflush(stdout);
		key = getch();

		switch (key) {
			case 'w':moveEntity(session,player,su);
				break;
 			case 's':moveEntity(session,player,giu);
				break;
			case 'a':moveEntity(session,player,sx);
				break;
			case 'd':moveEntity(session,player,dx);
				break;
			case 'q':session->state = finished;
				 stopAudio(bgmMusic);
				break;
		}
		if (playerPos->x == goalPos->x && playerPos->y == goalPos->y) {
			struct audioStream *victoryMusic = playAudio("Victory.waw");
			stopAudio(bgmMusic);
			struct map *map = loadMap("win.map");
			printEntireMap(map);
			printf("Premi q per continuare: \n");
			char tmp = getch();
			session->state = finished;
			stopAudio(victoryMusic);
			break;
		}
	} while (session->state == running);

	printf("\n");
	free(goalPos);
}

//------------------------------------------------------------------


//menu management--------------------------------------------------
void freeMenu(struct menu *menuToDelete) {
	for (int i = 0; i < menuToDelete->len; ++i) {
		//controllo se il menu ha dei sotto menu
		//se si ci entro dentro e li cancello
		if (menuToDelete->vociMenu[i].subMenu != NULL)
			freeMenu(menuToDelete->vociMenu[i].subMenu);
	}
	//elimino la lista delle voci del menu
	free(menuToDelete->vociMenu);
	//elimino il menu
	free(menuToDelete);
};

struct menu *initMenu() {
	struct menu *tmp = malloc(sizeof(struct menu));

	tmp->len = 4; //4 voci: 1 titolo + 3 opzioni
	tmp->vociMenu = malloc(sizeof(struct voceMenu)*tmp->len);
	tmp->vociMenu[0].testo = ".:Menu Principale:.\n";
	tmp->vociMenu[0].subMenu = NULL;
	tmp->vociMenu[0].action = NULL;

	tmp->vociMenu[1].testo = "1-Generazione mappe\n";
//---------------------Sub menu Generazione mappe----------------------------------
	tmp->vociMenu[1].subMenu = malloc(sizeof(struct menu)); //submenu gen mappe
	tmp->vociMenu[1].subMenu->len = 4; //4 voci: 1 titolo + 3 opzioni
	tmp->vociMenu[1].subMenu->vociMenu = malloc(sizeof(struct voceMenu)*4);

	//titolo
	tmp->vociMenu[1].subMenu->vociMenu[0].testo = ".:Generazione mappe:.\n";
	tmp->vociMenu[1].subMenu->vociMenu[0].subMenu = NULL;
	tmp->vociMenu[1].subMenu->vociMenu[0].action = NULL;


	tmp->vociMenu[1].subMenu->vociMenu[1].testo = "1-Genera e salva\n";
	tmp->vociMenu[1].subMenu->vociMenu[1].subMenu = NULL;
	tmp->vociMenu[1].subMenu->vociMenu[1].action = genAndSave;

	tmp->vociMenu[1].subMenu->vociMenu[2].testo = "2-Genera e gioca\n";
	tmp->vociMenu[1].subMenu->vociMenu[2].subMenu = NULL;
	tmp->vociMenu[1].subMenu->vociMenu[2].action = genAndPlay;

	tmp->vociMenu[1].subMenu->vociMenu[3].testo = "q-Esci\n";
	tmp->vociMenu[1].subMenu->vociMenu[3].subMenu = NULL;
	tmp->vociMenu[1].subMenu->vociMenu[3].action = esci;

	tmp->vociMenu[1].action = NULL;
//------------------------------------------------------------------------
	tmp->vociMenu[2].testo = "2-Carica mappa e gioca\n";
	tmp->vociMenu[2].subMenu = NULL; //submenu mappe
	tmp->vociMenu[2].action = loadAndPlay;

	tmp->vociMenu[3].testo = "q-Esci\n";
	tmp->vociMenu[3].subMenu = NULL;
	tmp->vociMenu[3].action = esci;

	if (DEBUG) printMenu(tmp);
	return tmp;
};


void printMenu(struct menu *menuToPrint) {
	debug_print("menuLen:%d menuAdd:%X \n",menuToPrint->len, menuToPrint);

	if (!DEBUG) printf("\e[1;1H\e[2J"); // POSIX clear screen ansi escape

	for (int i = 0; i < menuToPrint->len; ++i) {
		debug_print("i:%d %s ",i,menuToPrint->vociMenu[i].testo);
		printf("%s",menuToPrint->vociMenu[i].testo);
	}
};

int execMenuOption(struct menu *menuToExec, int option) {
	int exitCode = 0;

	//se esiste un binding di funzione questa viene chiamata ed eseguita
	if (menuToExec->vociMenu[option].action) {
		exitCode = menuToExec->vociMenu[option].action();
		debug_print("Funzione menu eseguita con exit code: %d\n",exitCode);
	}
	//se esiste un submenu viene gestito
	if (menuToExec->vociMenu[option].subMenu) {
		manMenu(menuToExec->vociMenu[option].subMenu);
		debug_print("Submenu chiamato\n");
	}

	return exitCode;
};

void manMenu(struct menu *menuToManage) {
	char choice[20];
	int shouldExit = 0;
	do {
		debug_print("Menu attivo: %s",menuToManage->vociMenu[0].testo);
		int selectionNotFound = 1;
		printMenu(menuToManage);

		printf("Seleziona un elemento: ");
		scanf("%s",choice);
		debug_print("L'utente ha inserito: %s\n",choice);
		strcat(choice,"-");

		for (int i = 0; i < menuToManage->len; ++i) {
			//se la voce è presente nel menu
			if (strstr(menuToManage->vociMenu[i].testo,choice)) {
				selectionNotFound = 0;
				shouldExit = execMenuOption(menuToManage,i);
			}
		}
		if (selectionNotFound) printf("Voce selezionata invalida: %s\n",choice);

	} while (!shouldExit);


}
//------------------------------------------------------------------------


//map management---------------------------------------------------------
struct map *loadMap(char *filename) {
	struct map *mappa = malloc(sizeof(struct map));
	FILE *input = fopen(filename, "r");
	if (!input) return NULL; //se il file non esiste o si è verificato un errore


	fscanf(input,"%d",&mappa->len);
	mappa->data = malloc(sizeof(enum mapBlocks)*mappa->len*mappa->len);

	debug_print("Dimensioni mappa: %d\n",mappa->len);

 	for (int i = 0; i < mappa->len; ++i) {
		for (int j = 0; j < mappa->len; ++j) {
			fscanf(input,"%u",&mappa->data[i*mappa->len+j]);
		}
	}
	fclose(input);
	debug_print("Mappa %s caricata\n",filename);
	return mappa;
};

void printMap(struct map *mappa, struct viewport *viewport) {
	debug_print("Stampando mappa (%d) viewport: @(%d,%d) %dx%d\n",mappa->len,viewport->posizione->x,viewport->posizione->y,viewport->dimensioni.x,viewport->dimensioni.y);

	for (int i = viewport->posizione->y - round(viewport->dimensioni.y/2); i < viewport->posizione->y + round(viewport->dimensioni.y/2); ++i) {
		for (int j = viewport->posizione->x - round(viewport->dimensioni.x/2); j < viewport->posizione->x + round(viewport->dimensioni.x/2); ++j) {
			if (j >= 0 && j < mappa->len && i >= 0 && i < mappa->len)printf("%s",mappa_blocchi[mappa->data[i*mappa->len + j]]);
			else printf("%s",mappa_blocchi[erba]);
		}
		printf("\n");
	}
}

void printEntireMap(struct map *mappa) {
	struct viewport pov;
	pov.posizione = malloc(sizeof(struct pos));

	pov.posizione->x = mappa->len/2;
	pov.posizione->y = mappa->len/2;
	pov.dimensioni.x = mappa->len+1;
	pov.dimensioni.y = mappa->len+1;

	printMap(mappa,&pov);

	free(pov.posizione);

};

void freeMap(struct map* mapToDel) {
	free(mapToDel->data);
	free(mapToDel);
};

void swapMapBlocks(struct map *mappa,struct pos blk1, struct pos blk2) {
	enum mapBlocks tmp = mappa->data[blk1.y*mappa->len + blk1.x];
	mappa->data[blk1.y*mappa->len + blk1.x] = mappa->data[blk2.y*mappa->len + blk2.x];
	mappa->data[blk2.y*mappa->len + blk2.x] = tmp;
};

struct map *genMap(int size) {
	if (size % 2 == 0) {
		printf("Errore: le dimensioni della mappa devono essere dispari\n");
		return 0;
	}

	struct map *mappa = malloc(sizeof(struct map));
	int *beenThere = calloc(sizeof(enum mapBlocks),size*size);
	struct pos posDig = {1,1};
	mappa->len = size;
	mappa->data = malloc(sizeof(enum mapBlocks)*size*size);

	for (int i = 0; i < size*size; ++i) mappa->data[i] = muro;

	digMap(mappa,posDig,beenThere);

	struct pos *exit;

	while (1) {
		exit = rndMapSpot(mappa,vuoto);

		if ((exit->x != 1) && (exit->y != 1)) {
			mappa->data[exit->y*mappa->len + exit->x] = end;
			debug_print("End at x: %d y: %d\n",exit->x,exit->y);
			mappa->data[1*mappa->len + 1] = spawnGiocatore; //posizione player (1;1)
			free(exit);
			break;
		}
		free(exit);
	}

	free(beenThere);
	return mappa;
};

struct pos *rndMapSpot(struct map *mappa, enum mapBlocks blockType) {
	int *validIndexes = malloc(sizeof(int)*mappa->len*mappa->len);
	int last = -1;
	struct pos *rndPos = malloc(sizeof(struct pos));

	for (int i = 0; i < mappa->len; ++i)
		for (int j = 0; j < mappa->len; ++j) {
			if (mappa->data[i*mappa->len + j] == blockType) validIndexes[++last] = i*mappa->len + j;
		}

	int tmp = validIndexes[rand() % last];
	rndPos->x = tmp %mappa->len;
	rndPos->y = tmp/mappa->len;

	debug_print("Random position: (%d,%d)\n",rndPos->x,rndPos->y);

	free(validIndexes);
	if (last >= 0) return rndPos;
	else return NULL;
};

void digMap(struct map *mappa, struct pos posizione, int *beenThere) {
	int locked = 0;
	int directions[4] = {dx,sx,su,giu};

	mappa->data[posizione.y*mappa->len + posizione.x] = vuoto;
	beenThere[posizione.y*mappa->len + posizione.x] = 1;

	shuffleArray(directions,4);

	for (int i = 0; i < 4; ++i) {
		switch (directions[i]) {
			case dx: if (posizione.x < mappa->len-3 && !beenThere[posizione.y*mappa->len + posizione.x+2]) { //destra
					mappa->data[posizione.y*mappa->len + posizione.x+1] = vuoto;

					posizione.x += 2;

					if (DEBUG) printEntireMap(mappa);
					debug_print("x: %d y: %d \n",posizione.x,posizione.y);
					digMap(mappa,posizione,beenThere);

				}
				break;
			case sx: if (posizione.x > 2 && !beenThere[posizione.y*mappa->len + posizione.x-2]){//sinistra
					mappa->data[posizione.y*mappa->len + posizione.x-1] = vuoto;

					posizione.x -= 2;

					if (DEBUG) printEntireMap(mappa);
					debug_print("x: %d y: %d \n",posizione.x,posizione.y);
					digMap(mappa,posizione,beenThere);

				}
				break;
			case su: if (posizione.y > 2 && !beenThere[(posizione.y-2)*mappa->len + posizione.x]){ //su
					mappa->data[(posizione.y-1)*mappa->len + posizione.x] = vuoto;

					posizione.y -= 2;

					if (DEBUG) printEntireMap(mappa);
					debug_print("x: %d y: %d \n",posizione.x,posizione.y);
					digMap(mappa,posizione,beenThere);

				}
				break;
			case giu: if (posizione.y < mappa->len-3 && !beenThere[(posizione.y+2)*mappa->len + posizione.x]){//giu
					mappa->data[(posizione.y+1)*mappa->len + posizione.x] = vuoto;

					posizione.y += 2;

					if (DEBUG) printEntireMap(mappa);
					debug_print("x: %d y: %d \n",posizione.x,posizione.y);
					digMap(mappa,posizione,beenThere);

				}
				break;
		}
	};
}

void saveMap(char *filename,struct map *mappa) {
	FILE *out = fopen(filename, "w");

	fprintf(out,"%d\n",mappa->len);

	for (int i = 0; i < mappa->len; ++i) {
		for (int j = 0; j < mappa->len; ++j) {
			fprintf(out,"%d ",mappa->data[i*mappa->len+j]);
		}
		fprintf(out,"\n");
	}
	fclose(out);
	debug_print("mappa salvata nel file %s\n",filename);
}

//---------------------------------------------------------------------------


//rendering ---------------------------------------------------------------
void renderMap(struct gameSession *sessione) {
	debug_print("Renderizzando mappa (%d) viewport: @(%d,%d) %dx%d\n",
			sessione->mappa->len,
			sessione->viewport->posizione->x,
			sessione->viewport->posizione->y,
			sessione->viewport->dimensioni.x,
			sessione->viewport->dimensioni.y);

	sessione->canvas->cursore.x = 0;
	sessione->canvas->cursore.y = 0;

	for (int i = sessione->viewport->posizione->y - sessione->viewport->dimensioni.y/2; i < sessione->viewport->posizione->y + sessione->viewport->dimensioni.y/2; ++i) {
		for (int j = sessione->viewport->posizione->x - sessione->viewport->dimensioni.x/2; j < sessione->viewport->posizione->x + sessione->viewport->dimensioni.x/2; ++j) {
			if (j >= 0 && j < sessione->mappa->len && i >= 0 && i < sessione->mappa->len)
				sessione->canvas->data[sessione->canvas->cursore.y*sessione->canvas->dim.x + sessione->canvas->cursore.x++] = mappa_blocchi[sessione->mappa->data[i*sessione->mappa->len + j]];
			else sessione->canvas->data[sessione->canvas->cursore.y*sessione->canvas->dim.x + sessione->canvas->cursore.x++] = mappa_blocchi[erba];
		}
		++sessione->canvas->cursore.y;
		sessione->canvas->cursore.x = 0;
	}
}

void renderEntities(struct gameSession *sessione) {
	sessione->canvas->cursore.x = 0;
	sessione->canvas->cursore.y = 0;

	for (int i = 0; i < sessione->poolEntita->maxEntities; ++i) {
		if (sessione->poolEntita->entities[i]) {
			debug_print("Renderizzando entita (%d) viewport: @(%d,%d) %dx%d Fac: %d\n",
				i,
				sessione->viewport->posizione->x,
				sessione->viewport->posizione->y,
				sessione->viewport->dimensioni.x,
				sessione->viewport->dimensioni.y,
				sessione->poolEntita->entities[i]->faction);


			int xCoord = sessione->poolEntita->entities[i]->posizione->x;
			int yCoord = sessione->poolEntita->entities[i]->posizione->y;
			//valori per normalizzare le coordinate dell'entità rispetto a quelle della canvas
			int normX = sessione->viewport->posizione->x - sessione->viewport->dimensioni.x/2;
			int normY = sessione->viewport->posizione->y - sessione->viewport->dimensioni.y/2;

			if (abs(xCoord - sessione->viewport->posizione->x) < sessione->viewport->dimensioni.x/2 &&
				abs(yCoord - sessione->viewport->posizione->y) < sessione->viewport->dimensioni.y/2) {
					sessione->canvas->data[(yCoord - normY)*sessione->canvas->dim.x + (xCoord - normX)] = sessione->poolEntita->entities[i]->img;
			}
		}
	}

};

void renderCanvas(struct canvas *canvas) {
	for (int i = 0; i < canvas->dim.y; ++i) {
		for (int j = 0; j < canvas->dim.x; ++j)
			printf("%s",canvas->data[i*canvas->dim.x + j]);
		printf("\n");
	};
}

void renderEventConsole(struct gameSession *session) {
	session->canvas->cursore.x = session->consoleViewport->relToCanvas->x;
	session->canvas->cursore.y = session->consoleViewport->relToCanvas->y;
	int tmp = session->eventConsole->lastEvent;

	for (int i = 0; i < session->eventConsole->maxEvents; ++i) {
		session->canvas->data[(session->canvas->cursore.y++)*session->canvas->dim.x + session->canvas->cursore.x] =
			session->eventConsole->buffer[((tmp+i)%MAX_EVENTS)];
	};
};
//---------------------------------------------------------------------------
