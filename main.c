#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspaudio.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

PSP_MODULE_INFO("ShadowFightLite", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SW 480
#define SH 272
#define BW 512
#define GY 220

static unsigned int __attribute__((aligned(64))) fb[BW * SH];

#define RGB(r,g,b) (0xFF000000u | ((unsigned)(r)<<16) | ((unsigned)(g)<<8) | (unsigned)(b))

/* ===== Примитивы ===== */
static void fillRect(int x,int y,int w,int h,unsigned int c){
    int x0=x,y0=y,x1=x+w,y1=y+h;
    if(x0<0)x0=0; if(y0<0)y0=0;
    if(x1>SW)x1=SW; if(y1>SH)y1=SH;
    if(x1<=x0||y1<=y0)return;
    for(int yy=y0;yy<y1;yy++){
        unsigned int* r=&fb[yy*BW+x0];
        int n=x1-x0; for(int i=0;i<n;i++) r[i]=c;
    }
}
static void px(int x,int y,unsigned int c){
    if(x<0||x>=SW||y<0||y>=SH)return;
    fb[y*BW+x]=c;
}
static void line(int x0,int y0,int x1,int y1,unsigned int c,int t){
    int dx=abs(x1-x0),dy=-abs(y1-y0);
    int sx=x0<x1?1:-1,sy=y0<y1?1:-1;
    int err=dx+dy,r=t/2;
    while(1){
        for(int oy=-r;oy<=r;oy++)for(int ox=-r;ox<=r;ox++)px(x0+ox,y0+oy,c);
        if(x0==x1&&y0==y1)break;
        int e2=2*err;
        if(e2>=dy){err+=dy;x0+=sx;}
        if(e2<=dx){err+=dx;y0+=sy;}
    }
}
static void circle(int cx,int cy,int r,unsigned int c){
    for(int y=-r;y<=r;y++){
        int w=(int)sqrtf((float)(r*r-y*y));
        for(int x=-w;x<=w;x++)px(cx+x,cy+y,c);
    }
}

/* ===== Шрифт 5x7 (латиница + кириллица) ===== */
typedef struct { unsigned char ch; unsigned char d[5]; } Glyph;
static const Glyph FONT[]={
    {' ',{0,0,0,0,0}},{'.',{0,0x60,0x60,0,0}},{'-',{0x08,0x08,0x08,0x08,0x08}},
    {':',{0,0x36,0x36,0,0}},{'/',{0x20,0x10,0x08,0x04,0x02}},{'!',{0,0,0x5F,0,0}},
    {'0',{0x3E,0x51,0x49,0x45,0x3E}},{'1',{0,0x42,0x7F,0x40,0}},
    {'2',{0x42,0x61,0x51,0x49,0x46}},{'3',{0x21,0x41,0x45,0x4B,0x31}},
    {'4',{0x18,0x14,0x12,0x7F,0x10}},{'5',{0x27,0x45,0x45,0x45,0x39}},
    {'6',{0x3C,0x4A,0x49,0x49,0x30}},{'7',{0x01,0x71,0x09,0x05,0x03}},
    {'8',{0x36,0x49,0x49,0x49,0x36}},{'9',{0x06,0x49,0x49,0x29,0x1E}},
    {'A',{0x7E,0x11,0x11,0x11,0x7E}},{'B',{0x7F,0x49,0x49,0x49,0x36}},
    {'C',{0x3E,0x41,0x41,0x41,0x22}},{'D',{0x7F,0x41,0x41,0x22,0x1C}},
    {'E',{0x7F,0x49,0x49,0x49,0x41}},{'F',{0x7F,0x09,0x09,0x09,0x01}},
    {'G',{0x3E,0x41,0x49,0x49,0x7A}},{'H',{0x7F,0x08,0x08,0x08,0x7F}},
    {'I',{0,0x41,0x7F,0x41,0}},{'J',{0x20,0x40,0x41,0x3F,0x01}},
    {'K',{0x7F,0x08,0x14,0x22,0x41}},{'L',{0x7F,0x40,0x40,0x40,0x40}},
    {'M',{0x7F,0x02,0x0C,0x02,0x7F}},{'N',{0x7F,0x04,0x08,0x10,0x7F}},
    {'O',{0x3E,0x41,0x41,0x41,0x3E}},{'P',{0x7F,0x09,0x09,0x09,0x06}},
    {'Q',{0x3E,0x41,0x51,0x21,0x5E}},{'R',{0x7F,0x09,0x19,0x29,0x46}},
    {'S',{0x46,0x49,0x49,0x49,0x31}},{'T',{0x01,0x01,0x7F,0x01,0x01}},
    {'U',{0x3F,0x40,0x40,0x40,0x3F}},{'V',{0x1F,0x20,0x40,0x20,0x1F}},
    {'W',{0x7F,0x20,0x18,0x20,0x7F}},{'X',{0x63,0x14,0x08,0x14,0x63}},
    {'Y',{0x03,0x04,0x78,0x04,0x03}},{'Z',{0x61,0x51,0x49,0x45,0x43}},
    /* Кириллица */
    {'*',{0x7E,0x11,0x11,0x11,0x7E}}, /* А */
    {'$',{0x7F,0x49,0x49,0x49,0x36}}, /* Б */
    {'@',{0x7F,0x49,0x49,0x49,0x41}}, /* В = как E вертикально */
    {'#',{0x3F,0x40,0x40,0x40,0x3F}}, /* Г — открытая U вверх */
    {'%',{0x3E,0x41,0x41,0x41,0x3E}}, /* Д упрощённо */
    {'^',{0x7F,0x40,0x40,0x40,0x40}}, /* Е = L зеркально */
    {'&',{0x63,0x14,0x08,0x14,0x63}}, /* Ж */
    {'(',{0x36,0x49,0x49,0x49,0x36}}, /* З */
    {')',{0x7F,0x08,0x08,0x08,0x7F}}, /* И */
    {'_',{0x3F,0x40,0x40,0x40,0x3F}}, /* К */
    {'+',{0x7F,0x08,0x08,0x08,0x40}}, /* Л упрощённо */
    {'=',{0x7F,0x08,0x14,0x22,0x41}}, /* М как K */
    {'[',{0x7F,0x41,0x41,0x41,0x41}}, /* Н */
    {']',{0x3E,0x41,0x41,0x41,0x3E}}, /* О */
    {'{',{0x7F,0x09,0x09,0x09,0x06}}, /* П как P */
    {'}',{0x7F,0x09,0x09,0x09,0x01}}, /* Р как F */
    {';',{0x46,0x49,0x49,0x49,0x31}}, /* С */
    {'<',{0x01,0x01,0x7F,0x01,0x01}}, /* Т */
    {'>',{0x3F,0x40,0x40,0x40,0x3F}}, /* У = U */
    {'?',{0x1F,0x20,0x40,0x20,0x1F}}, /* Ф как V */
    {'`',{0x63,0x14,0x08,0x14,0x63}}, /* Х как X */
    {'~',{0x7E,0x11,0x11,0x11,0x7E}}, /* Ц упрощённо */
    {'\\',{0x7F,0x20,0x18,0x20,0x7F}}, /* Ч как W */
    {'|',{0x3F,0x40,0x40,0x40,0x3F}}, /* Ш упрощённо */
    {'"',{0x01,0x01,0x7F,0x01,0x01}}, /* Щ упрощённо */
    {'\'',{0x7F,0x40,0x40,0x40,0x40}}, /* Ь */
};
static unsigned char glyphLookup(char c){
    if(c>='a'&&c<='z') c-=32;
    for(unsigned int i=0;i<sizeof(FONT)/sizeof(FONT[0]);i++)
        if(FONT[i].ch==(unsigned char)c) return i;
    return 0;
}
static void drawChar(int x,int y,char ch,unsigned int col,int s){
    unsigned char idx=glyphLookup(ch);
    const unsigned char* g=FONT[idx].d;
    for(int c=0;c<5;c++){
        unsigned char b=g[c];
        for(int r=0;r<7;r++) if(b&(1<<r)) fillRect(x+c*s,y+r*s,s,s,col);
    }
}
static void drawText(int x,int y,const char* t,unsigned int col,int s){
    int cx=x;
    while(*t){
        if(*t=='\n'){cx=x;y+=8*s;t++;continue;}
        drawChar(cx,y,*t,col,s); cx+=6*s; t++;
    }
}
static int textW(const char* t,int s){
    int w=0,mx=0;
    while(*t){
        if(*t=='\n'){if(w>mx)mx=w;w=0;t++;continue;}
        w+=6*s;t++;
    }
    return w>mx?w:mx;
}
static void drawTextC(int y,const char* t,unsigned int col,int s){
    int w=textW(t,s);
    drawText((SW-w)/2,y,t,col,s);
}

/* Русские строки через транслит-латиницу (простые) */
#define TXT_FIGHT    "\x2A \x2A \x2A"  /* placeholder — используй латиницу ниже */
/* Используем латиницу для читаемости */
const char* T_MENU_FIGHT = "FIGHT";
const char* T_MENU_TRAIN = "TRAINING";
const char* T_MENU_LEARN = "LEARN MOVES";
const char* T_MENU_ARSEN = "ARSENAL";

/* ===== Оружие (11 видов Акта I) ===== */
typedef struct {
    const char* name;
    int type;  /* 0 knife 1 fist 2 sai 3 baton 4 sword 5 machete 6 dagger 7 sting 8 scythe 9 tonfa 10 nunchaku */
    int dmg,range;
    float dur,hs,he,cd;
} WeaponDef;
static const WeaponDef WEAPONS[11]={
    {"KNIVES",    0, 6,64,0.24f,0.05f,0.14f,0.24f},
    {"FISTS",     1, 5,54,0.20f,0.04f,0.11f,0.19f},
    {"SAI",       2, 7,66,0.28f,0.06f,0.16f,0.30f},
    {"BATONS",    3, 9,70,0.36f,0.09f,0.20f,0.42f},
    {"NINJA SWD", 4,10,80,0.32f,0.08f,0.18f,0.38f},
    {"MACHETE",   5,12,74,0.40f,0.11f,0.22f,0.48f},
    {"DAGGERS",   6, 8,60,0.22f,0.05f,0.13f,0.24f},
    {"STING",     7, 9,72,0.26f,0.06f,0.15f,0.28f},
    {"BLOODREAP", 8,14,88,0.45f,0.12f,0.26f,0.55f},
    {"TONFAS",    9, 8,66,0.26f,0.06f,0.15f,0.28f},
    {"NUNCHAKU", 10, 7,74,0.30f,0.08f,0.17f,0.32f},
};

/* ===== Противники: 5 телохранителей + босс ===== */
typedef struct {
    const char* name;
    int hp,dmg,range,wt;
    float speed,cd;
    unsigned int color;
    int boss;
} EnemyDef;
static const EnemyDef ENEMIES[6]={
    {"SHIN",     60, 6,64,0,130.0f,0.95f,RGB(124,124,144),0},
    {"KIRPICH",  85, 9,70,3,140.0f,0.90f,RGB(141,122,104),0},
    {"IGLA",     80, 8,66,2,170.0f,0.75f,RGB(160,110,200),0},
    {"PRIZRAK", 100,10,80,4,175.0f,0.72f,RGB(108,168,216),0},
    {"SCHEGOL", 120,11,74,5,190.0f,0.65f,RGB(216,168,80),0},
    {"RYS",     180,14,88,8,210.0f,0.58f,RGB(217,144,60),1},
};

/* ===== Локации тренировки ===== */
typedef struct {
    const char* name;
    unsigned int skyTop,skyMid,skyBot,wall,ground,accent;
} LocDef;
static const LocDef LOCS[3]={
    {"YARD",   RGB(10,13,26),RGB(26,16,48),RGB(44,16,56),RGB(20,12,34),RGB(12,7,20),RGB(123,63,255)},
    {"ROOF",   RGB(5,8,21),  RGB(15,26,53),RGB(30,20,64),RGB(10,10,24),RGB(6,8,16), RGB(79,163,255)},
    {"CAVE",   RGB(21,8,8),  RGB(42,16,8), RGB(51,26,8), RGB(26,10,5), RGB(16,7,5), RGB(255,139,58)},
};

/* ===== Приёмы ===== */
typedef struct {
    const char* name;
    int id;
    int done;
} MoveDef;
static MoveDef MOVES[7]={
    {"PUNCH",     0,0},{"KICK",   1,0},{"BLOCK",  2,0},
    {"JUMP",      3,0},{"COMBO2", 4,0},{"COMBO3", 5,0},{"JUMPATK",6,0},
};

/* ===== Боец ===== */
typedef struct {
    float x,y,vx,vy;
    int hp,maxHp;
    int facing,onGround,blocking,hitDone;
    float attackT,cdT,stunT,hurtT;
    unsigned int color;
    int isPlayer,isBoss;
    int wt,dmg,range;
    float dur,hs,he,maxCd,speed;
    float aiT;
    int aiMode;
    int cfgIdx;
} Fighter;

/* ===== Груша ===== */
typedef struct {
    float x,y;
    float swing,swingV;
    float hitFlash;
} Bag;

/* ===== Состояние ===== */
#define ST_MENU     0
#define ST_WEAPONS  1
#define ST_CAMPAIGN 2
#define ST_TRAINING 3
#define ST_LEARN    4
#define ST_FIGHT    5
#define ST_END      6

static int gState=ST_MENU;
static int gMode=0;       /* 0 battle, 1 training, 2 learnmode */
static int gWeapon=0;
static int gEnemyIdx=0;
static int gLocIdx=0;
static int gUnlockedEnemies=1;
static int gUnlockedMoves[7]={1,0,0,0,0,0,0};
static int gCurrentMoveId=-1;
static Fighter gPlayer,gEnemy;
static Bag gBag;
static int gBagActive=0;
static float gMsgT=0;
static char gMsg[128];
static float gShakeT=0;
static int gTransition=0;
static int gPlayerWon=0;
static int gComboCount=0;
static float gComboT=0;
static float gBagHoldT=0;

static int gWasCross=0,gWasLeft=0,gWasRight=0,gWasUp=0,gWasDown=0,gWasCircle=0,gWasSquare=0;
static unsigned int gLastUs=0;

/* ===== Звук ===== */
#define AUDIO_BUF 1024
static short gAudioBuf[AUDIO_BUF*2];
static int gAudioReady=0;
static int gToneFreq=0;
static int gToneLeft=0;

static void audioInit(void){
    if(sceAudioSRCChReserve(AUDIO_BUF,44100,2)<0) return;
    gAudioReady=1;
    memset(gAudioBuf,0,sizeof(gAudioBuf));
}

static void audioPlayTone(int freq,int ms){
    gToneFreq=freq;
    gToneLeft=ms;
}

static void audioTick(void){
    if(!gAudioReady) return;
    /* генерируем буфер с текущим тоном */
    for(int i=0;i<AUDIO_BUF;i++){
        short s=0;
        if(gToneLeft>0){
            int period=44100/gToneFreq;
            if(period<2)period=2;
            s=((i%period)<(period/2))?6000:-6000;
        }
        gAudioBuf[i*2]=s;
        gAudioBuf[i*2+1]=s;
    }
    sceAudioSRCOutputBlocking(0x8000,gAudioBuf);
    if(gToneLeft>0) gToneLeft-=(AUDIO_BUF*1000/44100);
}

/* ===== Время ===== */
static float dt(void){
    unsigned int now=sceKernelGetSystemTimeLow();
    float d=(now-gLastUs)/1000000.0f;
    gLastUs=now;
    if(d>0.05f)d=0.05f;
    if(d<0)d=0;
    return d;
}

static void setMsg(const char* t,float time){
    strncpy(gMsg,t,sizeof(gMsg)-1);
    gMsg[sizeof(gMsg)-1]=0;
    gMsgT=time;
}

/* ===== Инициализация бойца ===== */
static void initPlayer(void){
    memset(&gPlayer,0,sizeof(gPlayer));
    const WeaponDef* w=&WEAPONS[gWeapon];
    gPlayer.isPlayer=1;
    gPlayer.hp=gPlayer.maxHp=100;
    gPlayer.x=SW*0.25f; gPlayer.y=GY;
    gPlayer.facing=1; gPlayer.onGround=1;
    gPlayer.color=RGB(26,10,42);
    gPlayer.wt=w->type; gPlayer.dmg=w->dmg; gPlayer.range=w->range;
    gPlayer.dur=w->dur; gPlayer.hs=w->hs; gPlayer.he=w->he;
    gPlayer.maxCd=w->cd;
    gPlayer.speed=0;
}
static void initEnemy(int idx){
    memset(&gEnemy,0,sizeof(gEnemy));
    const EnemyDef* e=&ENEMIES[idx];
    gEnemy.isPlayer=0;
    gEnemy.hp=gEnemy.maxHp=e->hp;
    gEnemy.x=SW*0.75f; gEnemy.y=GY;
    gEnemy.facing=-1; gEnemy.onGround=1;
    gEnemy.color=e->color;
    gEnemy.isBoss=e->boss;
    gEnemy.wt=e->wt; gEnemy.dmg=e->dmg; gEnemy.range=e->range;
    gEnemy.dur=0.32f; gEnemy.hs=0.09f; gEnemy.he=0.20f;
    gEnemy.maxCd=e->cd;
    gEnemy.speed=e->speed;
    gEnemy.cfgIdx=idx;
}
static void initBag(void){
    memset(&gBag,0,sizeof(gBag));
    gBag.x=SW*0.7f; gBag.y=GY;
    gBagActive=1;
}

static void startBattle(int idx){
    gEnemyIdx=idx;
    initPlayer();
    initEnemy(idx);
    gBagActive=0;
    gTransition=0; gComboCount=0; gComboT=0;
    gState=ST_FIGHT; gMode=0;
    char b[64];
    snprintf(b,sizeof(b),"ROUND %d\n%s",idx+1,ENEMIES[idx].name);
    setMsg(b,1.7f);
    audioPlayTone(440,200);
}
static void startTraining(int loc){
    gLocIdx=loc;
    initPlayer();
    initBag();
    gTransition=0; gComboCount=0; gComboT=0;
    gState=ST_FIGHT; gMode=1;
    setMsg(LOCS[loc].name,1.5f);
}
static void startLearn(int moveId){
    gCurrentMoveId=moveId;
    initPlayer();
    initBag();
    gBagHoldT=0;
    gTransition=0; gComboCount=0; gComboT=0;
    gState=ST_FIGHT; gMode=2;
    setMsg(MOVES[moveId].name,1.5f);
}

/* ===== Физика ===== */
static void physics(Fighter* f,float d){
    f->vy+=1800.0f*d;
    f->x+=f->vx*d;
    f->y+=f->vy*d;
    if(f->y>=GY){f->y=GY;f->vy=0;f->onGround=1;}
    else f->onGround=0;
    if(f->x<28){f->x=28;if(f->vx<0)f->vx=0;}
    if(f->x>SW-28){f->x=SW-28;if(f->vx>0)f->vx=0;}
    if(f->onGround && f->stunT<=0) f->vx*=0.85f;
}

/* ===== Попадания ===== */
static void checkHit(Fighter* a,Fighter* b){
    if(a->attackT<=0||a->hitDone)return;
    float el=a->dur-a->attackT;
    if(el<a->hs||el>a->he)return;
    float dx=b->x-a->x;
    if(fabsf(dx)>a->range)return;
    if((dx>0?1:-1)!=a->facing && fabsf(dx)>8)return;
    if(fabsf(a->y-b->y)>80)return;
    a->hitDone=1;
    float dmg=(float)a->dmg;
    if(b->blocking){dmg*=0.22f;b->vx=a->facing*80;b->stunT=0.10f;}
    else{b->vx=a->facing*190;b->stunT=0.26f;b->hurtT=0.22f;}
    b->hp-=(int)dmg; if(b->hp<0)b->hp=0;
    gShakeT=0.14f;
    gComboCount++; gComboT=1.6f;
    audioPlayTone(b->blocking?880:220,60);
}
static void checkHitBag(Fighter* a){
    if(!gBagActive) return;
    if(a->attackT<=0||a->hitDone)return;
    float el=a->dur-a->attackT;
    if(el<a->hs||el>a->he)return;
    float dx=gBag.x-a->x;
    if(fabsf(dx)>a->range)return;
    if((dx>0?1:-1)!=a->facing && fabsf(dx)>8)return;
    if(fabsf(a->y-gBag.y)>120)return;
    a->hitDone=1;
    gBag.swingV+=a->facing*3.5f;
    gBag.hitFlash=1.0f;
    gComboCount++; gComboT=1.6f;
    audioPlayTone(330,50);
}

/* ===== ИИ ===== */
static void enemyAI(float d){
    Fighter* e=&gEnemy; Fighter* p=&gPlayer;
    if(e->stunT>0)return;
    float dx=p->x-e->x;
    float dist=fabsf(dx);
    int dir=dx>0?1:-1;
    e->aiT-=d;
    if(e->aiT<=0){
        e->aiT=e->isBoss?(0.18f+(rand()%25)/100.0f):(0.28f+(rand()%40)/100.0f);
        if(dist<=e->range*0.92f && e->cdT<=0 && (rand()%100)<(e->isBoss?88:72))
            e->aiMode=2;
        else if(dist>e->range*1.05f) e->aiMode=0;
        else {
            int r=rand()%100;
            if(r<22) e->aiMode=1;
            else if(r<45) e->aiMode=3;
            else e->aiMode=0;
        }
    }
    e->blocking=0;
    if(e->aiMode==0){ e->vx=dir*e->speed; }
    else if(e->aiMode==2){
        e->vx*=0.7f;
        if(e->cdT<=0 && dist<=e->range){
            e->attackT=e->dur; e->cdT=e->maxCd; e->hitDone=0;
            audioPlayTone(440,50);
        }
    }
    else if(e->aiMode==1){ e->blocking=1; e->vx=-dir*40; }
    else e->vx*=0.85f;
}

/* ===== Обновление ===== */
static void update(float d){
    if(gState!=ST_FIGHT)return;
    Fighter* p=&gPlayer;

    if(gBagActive){
        p->facing=(gBag.x>=p->x)?1:-1;
    } else {
        Fighter* e=&gEnemy;
        if(p->attackT<=0&&p->stunT<=0) p->facing=(e->x>=p->x)?1:-1;
        if(e->attackT<=0&&e->stunT<=0) e->facing=(p->x>=e->x)?1:-1;
    }

    p->cdT-=d; if(p->cdT<0)p->cdT=0;
    p->stunT-=d; if(p->stunT<0)p->stunT=0;
    p->hurtT-=d; if(p->hurtT<0)p->hurtT=0;
    if(p->attackT>0){p->attackT-=d; if(p->attackT<0)p->attackT=0;}

    if(p->stunT>0){p->vx*=0.9f;}
    else if(p->attackT>0){p->vx*=0.84f;}
    else if(gWasSquare){p->vx*=0.7f; p->blocking=1;}
    else {
        p->blocking=0;
        float ax=0;
        if(gWasLeft)ax-=1;
        if(gWasRight)ax+=1;
        p->vx=ax*280;
    }

    if(!gBagActive){
        Fighter* e=&gEnemy;
        e->cdT-=d; if(e->cdT<0)e->cdT=0;
        e->stunT-=d; if(e->stunT<0)e->stunT=0;
        e->hurtT-=d; if(e->hurtT<0)e->hurtT=0;
        if(e->attackT>0){e->attackT-=d; if(e->attackT<0)e->attackT=0;}
        enemyAI(d);
        physics(e,d);
    }

    physics(p,d);

    if(gBagActive){
        gBag.swingV*=0.94f;
        gBag.swing+=gBag.swingV*d;
        gBag.swing*=0.985f;
        if(gBag.swing>0.9f)gBag.swing=0.9f;
        if(gBag.swing<-0.9f)gBag.swing=-0.9f;
        gBag.hitFlash-=d*3;
        if(gBag.hitFlash<0)gBag.hitFlash=0;
        checkHitBag(p);
    } else {
        checkHit(p,&gEnemy);
        checkHit(&gEnemy,p);
    }

    if(gShakeT>0)gShakeT-=d;
    if(gMsgT>0)gMsgT-=d;
    if(gComboT>0){gComboT-=d; if(gComboT<=0)gComboCount=0;}

    if(gMode==2 && gCurrentMoveId>=0 && !gUnlockedMoves[gCurrentMoveId]){
        if(gCurrentMoveId==2){
            if(gWasSquare){ gBagHoldT+=d; if(gBagHoldT>0.5f) gUnlockedMoves[2]=1; }
            else gBagHoldT=0;
        } else if(gCurrentMoveId==3){
            if(!gPlayer.onGround) gUnlockedMoves[3]=1;
        } else if(gCurrentMoveId==0){
            if(gPlayer.attackT>0) gUnlockedMoves[0]=1;
        } else if(gCurrentMoveId==1){
            /* handled via flag */
        } else if(gCurrentMoveId==6){
            if(!gPlayer.onGround && gPlayer.attackT>0) gUnlockedMoves[6]=1;
        }
    }

    if(!gTransition){
        if(!gBagActive){
            if(gEnemy.hp<=0){
                gTransition=1;
                if(gEnemyIdx<5){
                    if(gUnlockedEnemies<gEnemyIdx+2) gUnlockedEnemies=gEnemyIdx+2;
                    setMsg("WIN!\nNEXT ENEMY...",1.4f);
                    audioPlayTone(660,300);
                } else {
                    setMsg("ACT I CLEARED",1.5f);
                    gPlayerWon=1;
                }
            } else if(p->hp<=0){
                gTransition=1;
                setMsg("DEFEAT",1.5f);
                gPlayerWon=0;
            }
        }
    } else if(gMsgT<=0){
        if(gBagActive){
            gState=ST_LEARN;
        } else if(gPlayer.hp<=0 || gEnemy.hp<=0){
            if(gEnemy.hp<=0 && gEnemyIdx<5) startBattle(gEnemyIdx+1);
            else gState=ST_END;
        }
    }
}

/* ===== Отрисовка оружия ===== */
static void drawWeapon(int wt,float hx,float hy){
    if(wt==0||wt==6||wt==7){
        line((int)hx,(int)hy,(int)hx+18,(int)hy-6,RGB(215,227,234),4);
        line((int)hx-5,(int)hy,(int)hx+4,(int)hy,RGB(90,74,58),5);
    } else if(wt==1){
        circle((int)hx,(int)hy,6,RGB(255,183,77));
        circle((int)hx,(int)hy,3,RGB(255,143,0));
    } else if(wt==2){
        line((int)hx-3,(int)hy,(int)hx+16,(int)hy,RGB(200,216,224),3);
        line((int)hx+13,(int)hy-5,(int)hx+13,(int)hy+3,RGB(200,216,224),2);
    } else if(wt==3){
        line((int)hx-4,(int)hy,(int)hx+26,(int)hy,RGB(90,74,58),6);
        circle((int)hx+24,(int)hy,4,RGB(42,42,42));
    } else if(wt==4){
        line((int)hx-2,(int)hy,(int)hx+36,(int)hy,RGB(208,220,232),4);
        line((int)hx+34,(int)hy-3,(int)hx+46,(int)hy,RGB(208,220,232),3);
        line((int)hx-8,(int)hy,(int)hx+2,(int)hy,RGB(122,90,58),5);
    } else if(wt==5){
        line((int)hx,(int)hy,(int)hx+38,(int)hy-3,RGB(200,208,216),5);
        line((int)hx-6,(int)hy,(int)hx+2,(int)hy,RGB(90,74,58),6);
    } else if(wt==8){
        line((int)hx-8,(int)hy,(int)hx+6,(int)hy-2,RGB(141,110,99),4);
        for(int a=-40;a<=30;a+=6){
            float r=13.0f, rad=a*3.14159f/180.0f;
            px((int)(hx+10+cosf(rad)*r),(int)(hy-6+sinf(rad)*r),RGB(230,238,245));
            px((int)(hx+11+cosf(rad)*r),(int)(hy-6+sinf(rad)*r),RGB(230,238,245));
        }
    } else if(wt==9){
        line((int)hx-3,(int)hy,(int)hx+20,(int)hy,RGB(138,106,74),4);
        line((int)hx+8,(int)hy,(int)hx+8,(int)hy+10,RGB(138,106,74),4);
    } else if(wt==10){
        line((int)hx-6,(int)hy,(int)hx+8,(int)hy,RGB(58,42,26),4);
        line((int)hx+8,(int)hy,(int)hx+18,(int)hy+5,RGB(136,136,136),1);
        line((int)hx+18,(int)hy+5,(int)hx+30,(int)hy+2,RGB(58,42,26),4);
    }
}

/* ===== Отрисовка бойца ===== */
static void drawFighter(Fighter* f){
    if(!f)return;
    int dir=f->facing;
    unsigned int col=f->isPlayer?RGB(26,10,42):(f->hurtT>0?RGB(255,91,91):f->color);
    float bx=f->x, by=f->y;

    for(int i=-22;i<=22;i++) px((int)bx+i,(int)by+2,RGB(20,20,20));

    if(f->isPlayer){
        /* аура */
        for(int i=-30;i<=30;i++)
            for(int j=-50;j<=50;j++){
                float d=sqrtf((float)(i*i+j*j));
                if(d<32 && d>26) px((int)bx+i,(int)(by-60+j),RGB(60,20,100));
            }
    }
    if(f->isBoss){
        for(int i=-30;i<=30;i++)
            for(int j=-50;j<=50;j++){
                float d=sqrtf((float)(i*i+j*j));
                if(d<32 && d>26) px((int)bx+i,(int)(by-60+j),RGB(120,50,10));
            }
    }

    float lx0=bx-5, ly0=by-32, lx1=bx-10, ly1=by-1;
    float rx0=bx+5, ly2=by-32, rx1=bx+10, ry1=by-1;
    if(!f->onGround){ lx1=bx-14; rx1=bx+14; }
    line((int)lx0,(int)ly0,(int)lx1,(int)ly1,col,6);
    line((int)rx0,(int)ly2,(int)rx1,(int)ry1,col,6);

    line((int)bx,(int)(by-60),(int)bx,(int)(by-30),col,14);
    circle((int)bx,(int)(by-72),9,col);

    /* глаза */
    unsigned int eyeCol = f->isPlayer?RGB(185,140,255):(f->isBoss?RGB(255,204,51):RGB(255,136,153));
    px((int)bx-3,(int)(by-74),eyeCol);
    px((int)bx-2,(int)(by-74),eyeCol);
    px((int)bx+3,(int)(by-74),eyeCol);
    px((int)bx+2,(int)(by-74),eyeCol);

    float frontX=bx+11, frontY=by-42;
    float backX=bx-10, backY=by-44;
    if(f->attackT>0){
        float prog=1.0f-(f->attackT/f->dur);
        float swing=sinf(prog*3.14159f);
        if(swing<0)swing=0; if(swing>1)swing=1;
        frontX=bx+9+(f->range*0.75f)*swing*dir;
        frontY=by-52+8*(1-swing);
    } else if(f->blocking){
        frontX=bx+12*dir; frontY=by-55;
        backX=bx+4*dir; backY=by-52;
    }
    line((int)bx,(int)(by-58),(int)backX,(int)backY,col,5);
    line((int)bx,(int)(by-58),(int)frontX,(int)frontY,col,6);

    drawWeapon(f->wt,frontX,frontY);
    if(f->wt==0||f->wt==6) drawWeapon(f->wt,backX,backY);

    if(f->hurtT>0) circle((int)bx,(int)(by-50),22,RGB(255,255,255));
}

/* ===== Отрисовка груши ===== */
static void drawBag(void){
    if(!gBagActive)return;
    float bx=gBag.x, by=gBag.y;
    for(int i=-20;i<=20;i++) px((int)bx+i,(int)by+2,RGB(20,20,20));

    line((int)bx,(int)(by-160),(int)bx,(int)(by-130+gBag.swing*20),RGB(100,100,100),2);

    float off=gBag.swing*30;
    float cy=by-100;
    unsigned int body = gBag.hitFlash>0?RGB(255,85,102):RGB(90,42,42);
    unsigned int edge = gBag.hitFlash>0?RGB(255,170,136):RGB(138,58,58);

    for(int j=-70;j<=70;j++){
        int w=(int)(28*(1-fabsf(j)/80.0f));
        if(w<2)w=2;
        for(int i=-w;i<=w;i++)
            px((int)(bx+off*(j+70)/140)+i,(int)(cy+j),body);
    }
    /* швы */
    line((int)(bx+off),(int)(cy-70),(int)(bx+off),(int)(cy+70),edge,2);
    line((int)(bx-28),(int)cy,(int)(bx+28),(int)cy,edge,2);
    circle((int)(bx+off),(int)(cy-70),6,RGB(58,26,26));
}

/* ===== Фон ===== */
static void drawBackground(void){
    const LocDef* loc;
    LocDef battleLoc = {0,RGB(10,13,26),RGB(26,16,48),RGB(44,16,56),RGB(20,12,34),RGB(12,7,20),RGB(123,63,255)};
    if(gMode==1||gMode==2) loc=&LOCS[gLocIdx];
    else loc=&battleLoc;

    for(int y=0;y<GY;y++){
        float t=(float)y/GY;
        unsigned int c;
        if(t<0.5f){
            float u=t*2.0f;
            int r=(int)((loc->skyTop>>16&0xFF)*(1-u)+(loc->skyMid>>16&0xFF)*u);
            int g=(int)((loc->skyTop>>8&0xFF)*(1-u)+(loc->skyMid>>8&0xFF)*u);
            int b=(int)((loc->skyTop&0xFF)*(1-u)+(loc->skyMid&0xFF)*u);
            c=RGB(r,g,b);
        } else {
            float u=(t-0.5f)*2.0f;
            int r=(int)((loc->skyMid>>16&0xFF)*(1-u)+(loc->skyBot>>16&0xFF)*u);
            int g=(int)((loc->skyMid>>8&0xFF)*(1-u)+(loc->skyBot>>8&0xFF)*u);
            int b=(int)((loc->skyMid&0xFF)*(1-u)+(loc->skyBot&0xFF)*u);
            c=RGB(r,g,b);
        }
        unsigned int* row=&fb[y*BW];
        for(int x=0;x<SW;x++) row[x]=c;
    }

    /* луна */
    int mx=SW-100,my=45;
    for(int y=-40;y<=40;y++)for(int x=-40;x<=40;x++){
        float d2=(float)(x*x+y*y);
        if(d2<1600.0f){
            float a=1.0f-sqrtf(d2)/40.0f;
            if(a<0)a=0;
            int base=fb[(my+y)*BW+mx+x]&0xFFFFFF;
            int br=(base>>16)&0xFF,bg=(base>>8)&0xFF,bb=base&0xFF;
            int nr=br+(int)((255-br)*a*0.35f);
            int ng=bg+(int)((235-bg)*a*0.35f);
            int nb=bb+(int)((190-bb)*a*0.35f);
            fb[(my+y)*BW+mx+x]=RGB(nr,ng,nb);
        }
    }
    circle(mx,my,20,RGB(255,242,205));

    /* силуэты */
    int hts[]={60,40,55,30,50,45,58,38};
    int step=SW/7;
    for(int i=0;i<8;i++){
        int x0=i*step-step/2;
        int top=GY-hts[i];
        fillRect(x0,top,step+2,GY-top,loc->wall);
    }
    fillRect(0,GY-3,SW,3,loc->accent);
    /* земля */
    fillRect(0,GY,SW,SH-GY,loc->ground);
}

/* ===== HUD ===== */
static void drawBar(int x,int y,int w,int h,float ratio,unsigned int c,int flip){
    if(ratio<0)ratio=0; if(ratio>1)ratio=1;
    fillRect(x-1,y-1,w+2,h+2,RGB(0,0,0));
    fillRect(x,y,w,h,RGB(40,20,60));
    int fw=(int)(w*ratio);
    if(flip) fillRect(x+(w-fw),y,fw,h,c);
    else     fillRect(x,y,fw,h,c);
}
static void drawHUD(void){
    if(gState!=ST_FIGHT)return;
    int bw=170,bh=10,top=12;
    drawBar(14,top,bw,bh,(float)gPlayer.hp/gPlayer.maxHp,RGB(79,195,247),0);
    if(!gBagActive){
        drawBar(SW-14-bw,top,bw,bh,(float)gEnemy.hp/gEnemy.maxHp,
            gEnemy.isBoss?RGB(255,68,68):RGB(255,82,82),1);
        drawText(14,top+bh+4,"YOU",RGB(160,140,210),1);
        int w=textW(ENEMIES[gEnemyIdx].name,1);
        drawText(SW-14-w,top+bh+4,ENEMIES[gEnemyIdx].name,RGB(160,140,210),1);
    }
    if(gComboCount>1){
        char b[16]; snprintf(b,sizeof(b),"x%d",gComboCount);
        int w=textW(b,2);
        drawText((SW-w)/2,60,b,RGB(255,200,60),2);
    }
    if(gMsgT>0){
        drawTextC(SH*0.32f+2,gMsg,RGB(0,0,0),2);
        drawTextC(SH*0.32f,gMsg,RGB(240,230,255),2);
    }
    if(gMode==2&&gCurrentMoveId>=0){
        drawTextC(SH-30,MOVES[gCurrentMoveId].name,RGB(200,180,255),1);
    }
}

/* ===== Экраны ===== */
static void drawGradBg(unsigned int t,unsigned int m,unsigned int b){
    for(int y=0;y<SH;y++){
        float u=(float)y/SH;
        unsigned int c;
        if(u<0.5f){
            float k=u*2;
            int r=(int)((t>>16&0xFF)*(1-k)+(m>>16&0xFF)*k);
            int g=(int)((t>>8&0xFF)*(1-k)+(m>>8&0xFF)*k);
            int bb=(int)((t&0xFF)*(1-k)+(m&0xFF)*k);
            c=RGB(r,g,bb);
        } else {
            float k=(u-0.5f)*2;
            int r=(int)((m>>16&0xFF)*(1-k)+(b>>16&0xFF)*k);
            int g=(int)((m>>8&0xFF)*(1-k)+(b>>8&0xFF)*k);
            int bb=(int)((m&0xFF)*(1-k)+(b&0xFF)*k);
            c=RGB(r,g,bb);
        }
        unsigned int* row=&fb[y*BW];
        for(int x=0;x<SW;x++) row[x]=c;
    }
}
static void drawMenu(void){
    drawGradBg(RGB(10,7,20),RGB(27,18,48),RGB(7,7,12));
    drawTextC(30,"SHADOW FIGHT",RGB(232,213,255),4);
    drawTextC(72,"LITE",RGB(232,213,255),4);
    drawTextC(110,"ACT I - PEREROZHDENIE",RGB(160,140,210),1);
    drawTextC(150,"X - FIGHT    O - TRAIN",RGB(240,230,255),1);
    drawTextC(170,"SQ - LEARN   TR - ARSENAL",RGB(240,230,255),1);
    drawTextC(220,"DPAD-X: ATTACK",RGB(140,130,170),1);
    drawTextC(234,"CIRCLE: JUMP",RGB(140,130,170),1);
    drawTextC(248,"SQUARE: BLOCK",RGB(140,130,170),1);
}
static void drawWeaponSelect(void){
    drawGradBg(RGB(10,7,20),RGB(27,18,48),RGB(7,7,12));
    drawTextC(8,"ARSENAL ACT I",RGB(232,213,255),2);
    for(int i=0;i<11;i++){
        int col=i%2;
        int row=i/2;
        int x=20+col*(SW/2-10);
        int y=30+row*22;
        unsigned int c=(i==gWeapon)?RGB(160,110,255):RGB(35,25,55);
        fillRect(x,y,SW/2-30,20,c);
        drawText(x+4,y+6,WEAPONS[i].name,RGB(255,255,255),1);
    }
    drawTextC(SH-16,"DPAD UP/DOWN  X - SELECT",RGB(160,140,210),1);
}
static void drawCampaign(void){
    drawGradBg(RGB(10,7,20),RGB(27,18,48),RGB(7,7,12));
    drawTextC(8,"ACT I - CAMPAIGN",RGB(232,213,255),2);
    for(int i=0;i<6;i++){
        int y=34+i*24;
        int unlocked=i<gUnlockedEnemies;
        int done=i<gUnlockedEnemies-1;
        unsigned int c=done?RGB(30,30,40):
            (ENEMIES[i].boss?RGB(80,20,20):(unlocked?RGB(50,40,80):RGB(20,15,30)));
        fillRect(20,y,SW-40,20,c);
        if(unlocked){
            drawText(28,y+6,ENEMIES[i].name,RGB(255,255,255),1);
            char hp[16]; snprintf(hp,sizeof(hp),"HP %d",ENEMIES[i].hp);
            drawText(SW-100,y+6,hp,RGB(200,180,220),1);
        } else {
            drawText(28,y+6,"LOCKED",RGB(100,90,120),1);
        }
    }
    drawTextC(SH-16,"X - START   O - BACK",RGB(160,140,210),1);
}
static void drawTrainingScreen(void){
    drawGradBg(RGB(10,7,20),RGB(27,18,48),RGB(7,7,12));
    drawTextC(20,"TRAINING",RGB(232,213,255),3);
    drawTextC(60,"CHOOSE LOCATION",RGB(160,140,210),1);
    for(int i=0;i<3;i++){
        int y=80+i*40;
        unsigned int c=(i==gLocIdx)?RGB(160,110,255):RGB(50,40,80);
        fillRect(40,y,SW-80,32,c);
        drawText(50,y+10,LOCS[i].name,RGB(255,255,255),2);
    }
    drawTextC(SH-16,"DPAD UP/DOWN  X - START",RGB(160,140,210),1);
}
static void drawLearnScreen(void){
    drawGradBg(RGB(10,7,20),RGB(27,18,48),RGB(7,7,12));
    drawTextC(8,"LEARN MOVES",RGB(232,213,255),2);
    for(int i=0;i<7;i++){
        int y=34+i*22;
        int unlocked=(i==0)||gUnlockedMoves[i-1];
        int done=gUnlockedMoves[i];
        unsigned int c=done?RGB(30,60,30):(unlocked?RGB(50,40,80):RGB(20,15,30));
        fillRect(20,y,SW-40,20,c);
        char b[32];
        snprintf(b,sizeof(b),"%s%s",done?"[V] ":"[ ] ",MOVES[i].name);
        drawText(28,y+6,b,unlocked?RGB(255,255,255):RGB(100,90,120),1);
    }
    drawTextC(SH-16,"DPAD  X - TRAIN",RGB(160,140,210),1);
}
static void drawEnd(void){
    drawGradBg(RGB(10,7,20),RGB(27,18,48),RGB(7,7,12));
    if(gPlayerWon){
        drawTextC(60,"ACT I CLEARED",RGB(232,213,255),3);
        drawTextC(120,"RYS DEFEATED",RGB(240,230,255),2);
    } else {
        drawTextC(60,"DEFEAT",RGB(255,120,120),3);
        char b[64]; snprintf(b,sizeof(b),"KILLED BY %s",ENEMIES[gEnemyIdx].name);
        drawTextC(120,b,RGB(200,180,220),1);
    }
    drawTextC(SH-30,"X - MENU",RGB(240,230,255),1);
}

/* ===== Ввод ===== */
static void readInput(void){
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad,1);
    int l=(pad.Buttons&PSP_CTRL_LEFT)!=0;
    int r=(pad.Buttons&PSP_CTRL_RIGHT)!=0;
    int u=(pad.Buttons&PSP_CTRL_UP)!=0;
    int d=(pad.Buttons&PSP_CTRL_DOWN)!=0;
    int x=(pad.Buttons&PSP_CTRL_CROSS)!=0;
    int o=(pad.Buttons&PSP_CTRL_CIRCLE)!=0;
    int sq=(pad.Buttons&PSP_CTRL_SQUARE)!=0;

    if(gState==ST_MENU){
        if(x&&!gWasCross) gState=ST_CAMPAIGN;
        else if(o&&!gWasCircle) gState=ST_TRAINING;
        else if(sq&&!gWasSquare) gState=ST_LEARN;
        /* SELECT/TRIANGLE для арсенала */
        if(pad.Buttons&PSP_CTRL_TRIANGLE) gState=ST_WEAPONS;
    } else if(gState==ST_WEAPONS){
        if(d&&!gWasDown) gWeapon=(gWeapon+1)%11;
        if(u&&!gWasUp)   gWeapon=(gWeapon+10)%11;
        if(x&&!gWasCross) gState=ST_MENU;
        if(o&&!gWasCircle) gState=ST_MENU;
    } else if(gState==ST_CAMPAIGN){
        if(x&&!gWasCross) startBattle(gUnlockedEnemies-1);
        if(o&&!gWasCircle) gState=ST_MENU;
    } else if(gState==ST_TRAINING){
        if(d&&!gWasDown) gLocIdx=(gLocIdx+1)%3;
        if(u&&!gWasUp)   gLocIdx=(gLocIdx+2)%3;
        if(x&&!gWasCross) startTraining(gLocIdx);
        if(o&&!gWasCircle) gState=ST_MENU;
    } else if(gState==ST_LEARN){
        if(d&&!gWasDown) { /* выбор приёма */ }
        if(x&&!gWasCross){
            /* найти первый незавершённый */
            for(int i=0;i<7;i++){
                if(!gUnlockedMoves[i] && (i==0||gUnlockedMoves[i-1])){
                    startLearn(i); break;
                }
            }
        }
        if(o&&!gWasCircle) gState=ST_MENU;
    } else if(gState==ST_FIGHT){
        gWasLeft=l; gWasRight=r; gWasSquare=sq;
        if(o&&!gWasUp){
            if(gPlayer.onGround&&gPlayer.stunT<=0&&gPlayer.attackT<=0){
                gPlayer.vy=-600; gPlayer.onGround=0;
                audioPlayTone(420,80);
            }
        }
        if(x&&!gWasCross){
            if(gPlayer.cdT<=0&&gPlayer.attackT<=0&&gPlayer.stunT<=0){
                gPlayer.attackT=gPlayer.dur;
                gPlayer.cdT=gPlayer.maxCd;
                gPlayer.hitDone=0;
                gPlayer.blocking=0;
                audioPlayTone(300,40);
            }
        }
    } else if(gState==ST_END){
        if(x&&!gWasCross) gState=ST_MENU;
    }
    gWasLeft=l; gWasRight=r; gWasUp=u; gWasDown=d;
    gWasCross=x; gWasCircle=o; gWasSquare=sq;
}

/* ===== main ===== */
int main(void){
    sceDisplaySetMode(0,SW,SH);
    sceDisplaySetFrameBuf((void*)fb,BW,PSP_DISPLAY_PIXEL_FORMAT_8888,
                          PSP_DISPLAY_SETBUF_IMMEDIATE);
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    audioInit();
    gLastUs=sceKernelGetSystemTimeLow();

    while(1){
        sceDisplayWaitVblankStart();
        float d=dt();
        readInput();
        update(d);
        audioTick();

        if(gState==ST_MENU)            drawMenu();
        else if(gState==ST_WEAPONS)    drawWeaponSelect();
        else if(gState==ST_CAMPAIGN)   drawCampaign();
        else if(gState==ST_TRAINING)   drawTrainingScreen();
        else if(gState==ST_LEARN)      drawLearnScreen();
        else if(gState==ST_END)        drawEnd();
        else if(gState==ST_FIGHT){
            drawBackground();
            if(gBagActive) drawBag();
            else drawFighter(&gEnemy);
            drawFighter(&gPlayer);
            drawHUD();
        }
    }
    return 0;
}
