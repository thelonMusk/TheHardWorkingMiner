#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <algorithm>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// ─────────────────────────────────────────
//  Constants
// ─────────────────────────────────────────
const int WIN_W      = 480;
const int WIN_H      = 700;
const int TILE       = 32;
const int GROUND_TOP = 180;
const int SURFACE_Y  = 148;
const int WORLD_ROWS = 300;
const int WORLD_COLS = WIN_W / TILE;  // 15 cols
const float REGEN_INTERVAL = 0.4f;

// ─────────────────────────────────────────
//  Rock types
// ─────────────────────────────────────────
enum RockType { DIRT=0, STONE, COAL, IRON, GOLD, RUBY, DIAMOND, EMPTY };

struct RockInfo { const char* name; SDL_Color color; int value; int minDepth; float rarity; };
static const RockInfo ROCKS[] = {
    {"Dirt",    {139, 90, 43,255},   0,   0, 0.00f},
    {"Stone",   {150,150,150,255},   1,   0, 0.55f},
    {"Coal",    { 50, 50, 50,255},   3,   5, 0.20f},
    {"Iron",    {210,140, 80,255},   8,  15, 0.12f},
    {"Gold",    {255,215,  0,255},  25,  30, 0.07f},
    {"Ruby",    {200,  0, 50,255},  60,  50, 0.04f},
    {"Diamond", {100,220,255,255}, 150,  80, 0.02f},
    {"Empty",   {  8,  4,  2,255},   0,   0, 0.00f},
};

// ─────────────────────────────────────────
//  World grid
// ─────────────────────────────────────────
RockType world[WORLD_ROWS][WORLD_COLS];

RockType genTile(int r) {
    float roll = (float)rand()/RAND_MAX;
    RockType chosen = (r < 3) ? DIRT : STONE;
    for (int t = DIAMOND; t >= COAL; t--) {
        if (r >= ROCKS[t].minDepth && roll < ROCKS[t].rarity) {
            chosen = (RockType)t; break;
        }
    }
    return chosen;
}

void generateWorld() {
    srand((unsigned)time(nullptr));
    for (int r = 0; r < WORLD_ROWS; r++)
        for (int c = 0; c < WORLD_COLS; c++)
            world[r][c] = genTile(r);
}

void regenOneTile() {
    std::vector<std::pair<int,int>> empties;
    for (int r = 0; r < WORLD_ROWS; r++)
        for (int c = 0; c < WORLD_COLS; c++)
            if (world[r][c] == EMPTY)
                empties.push_back({r,c});
    if (empties.empty()) return;
    auto& e = empties[rand() % empties.size()];
    world[e.first][e.second] = genTile(e.first);
}

// ─────────────────────────────────────────
//  SDL Globals
// ─────────────────────────────────────────
SDL_Window*   gWindow   = nullptr;
SDL_Renderer* gRenderer = nullptr;
TTF_Font*     gFont16   = nullptr;
TTF_Font*     gFont14   = nullptr;
TTF_Font*     gFont13   = nullptr;
TTF_Font*     gFont12   = nullptr;
TTF_Font*     gFont11   = nullptr;
TTF_Font*     gFont26   = nullptr;
TTF_Font*     gFont17   = nullptr;

// ─────────────────────────────────────────
//  Draw helpers
// ─────────────────────────────────────────
void setColor(SDL_Color c) {
    SDL_SetRenderDrawColor(gRenderer, c.r, c.g, c.b, c.a);
}
void setColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a=255) {
    SDL_SetRenderDrawColor(gRenderer, r, g, b, a);
}

void fillRect(int x, int y, int w, int h, SDL_Color c) {
    setColor(c);
    SDL_Rect r = {x,y,w,h};
    SDL_RenderFillRect(gRenderer, &r);
}
void fillRect(int x, int y, int w, int h, Uint8 r, Uint8 g, Uint8 b) {
    setColor(r,g,b);
    SDL_Rect rc = {x,y,w,h};
    SDL_RenderFillRect(gRenderer, &rc);
}

void outlineRect(int x, int y, int w, int h, Uint8 r, Uint8 g, Uint8 b) {
    setColor(r,g,b);
    SDL_Rect rc = {x,y,w,h};
    SDL_RenderDrawRect(gRenderer, &rc);
}
void outlineRect(int x, int y, int w, int h, SDL_Color c) {
    setColor(c);
    SDL_Rect rc = {x,y,w,h};
    SDL_RenderDrawRect(gRenderer, &rc);
}

TTF_Font* pickFont(int sz) {
    if (sz >= 24) return gFont26;
    if (sz >= 17) return gFont17;
    if (sz >= 16) return gFont16;
    if (sz >= 14) return gFont14;
    if (sz >= 13) return gFont13;
    if (sz >= 12) return gFont12;
    return gFont11;
}

void drawText(const std::string& s, int x, int y, SDL_Color c, int sz=16) {
    TTF_Font* font = pickFont(sz);
    if (!font || s.empty()) return;
    SDL_Surface* surf = TTF_RenderText_Blended(font, s.c_str(), c);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(gRenderer, surf);
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(gRenderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

void drawButton(int x, int y, int w, int h,
                Uint8 br, Uint8 bg2, Uint8 bb,
                Uint8 or_, Uint8 og, Uint8 ob,
                const std::string& label,
                SDL_Color tc={255,255,255,255}, int ts=14) {
    fillRect(x,y,w,h,br,bg2,bb);
    outlineRect(x,y,w,h,or_,og,ob);
    drawText(label, x+10, y+h/2-ts/2, tc, ts);
}

bool inRect(int mx, int my, int x, int y, int w, int h) {
    return mx>=x && mx<=x+w && my>=y && my<=y+h;
}

// ─────────────────────────────────────────
//  Game
// ─────────────────────────────────────────
enum State { S_SURFACE, S_DRILLING, S_RETURNING, S_SHOP };
struct OreEntry { RockType type; int value; };
struct Particle { float x,y,vx,vy,life; SDL_Color col; };

struct Game {
    State state = S_SURFACE;
    int   coins = 0;

    float minerX     = WIN_W/2.f;
    bool  movingLeft = false, movingRight = false;
    const float MINER_SPEED = 190.f;

    float drillX = WIN_W/2.f;
    float drillY = (float)SURFACE_Y;
    float camY   = 0;
    float depthM = 0;

    int speedLvl=1, batteryLvl=1, bagLvl=1;
    float getDrillSpeed(){ return 65.f+speedLvl*40.f; }
    int   getMaxDepth()  { return 20+batteryLvl*30; }
    int   getBagCap()    { return 5+bagLvl*5; }

    std::vector<OreEntry> bag;
    std::vector<Particle> particles;
    float regenTimer = 0;
    Uint32 lastTick  = 0;

    void init() {
        lastTick = SDL_GetTicks();
        generateWorld();
    }

    float getDt() {
        Uint32 now = SDL_GetTicks();
        float dt = (now - lastTick) / 1000.f;
        lastTick = now;
        return (dt > 0.1f) ? 0.1f : dt;
    }

    void startDrill() {
        bag.clear();
        drillX=minerX; drillY=(float)SURFACE_Y;
        camY=0; depthM=0;
        state=S_DRILLING;
    }

    void spawnParticles(float wx, float wy, SDL_Color col) {
        for (int p=0; p<6; p++) {
            float a=(rand()%360)*3.14159f/180.f;
            float s=25.f+rand()%55;
            particles.push_back({wx, wy-camY, cosf(a)*s, sinf(a)*s, 0.5f, col});
        }
    }

    void update(float dt) {
        if (state==S_SURFACE) {
            if (movingLeft)  minerX-=MINER_SPEED*dt;
            if (movingRight) minerX+=MINER_SPEED*dt;
            minerX=std::max(16.f,std::min((float)(WIN_W-16),minerX));
            regenTimer+=dt;
            if (regenTimer>=REGEN_INTERVAL) { regenTimer=0; regenOneTile(); }
        }

        if (state==S_DRILLING) {
            drillY+=getDrillSpeed()*dt;
            float worldY=drillY-GROUND_TOP+camY;
            int row=(int)(worldY/TILE);
            int col=(int)(drillX/TILE);
            col=std::max(0,std::min(WORLD_COLS-1,col));
            if (row>=0 && row<WORLD_ROWS) {
                RockType rt=world[row][col];
                if (rt!=EMPTY) {
                    if (rt!=DIRT && (int)bag.size()<getBagCap()) {
                        bag.push_back({rt,ROCKS[rt].value});
                        spawnParticles(drillX,drillY,ROCKS[rt].color);
                    }
                    world[row][col]=EMPTY;
                }
            }
            float screenY=drillY-camY;
            if (screenY>WIN_H*0.62f) camY+=getDrillSpeed()*dt;
            depthM=std::max(0.f,(drillY-GROUND_TOP+camY)/TILE);
            if (row>=getMaxDepth()||(int)bag.size()>=getBagCap())
                state=S_RETURNING;
        }

        if (state==S_RETURNING) {
            drillY-=getDrillSpeed()*1.6f*dt;
            float screenY=drillY-camY;
            if (screenY<WIN_H*0.38f&&camY>0)
                camY-=getDrillSpeed()*1.6f*dt;
            if (camY<0) camY=0;
            if (drillY<=SURFACE_Y) {
                drillY=(float)SURFACE_Y;
                minerX=drillX;
                for (auto& o:bag) coins+=o.value;
                bag.clear();
                state=S_SURFACE; camY=0;
            }
        }

        for (auto& p:particles){
            p.x+=p.vx*dt; p.y+=p.vy*dt;
            p.vy+=80.f*dt; p.life-=dt;
        }
        particles.erase(
            std::remove_if(particles.begin(),particles.end(),
                [](const Particle& p){return p.life<=0;}),
            particles.end());
    }
} G;

// ─────────────────────────────────────────
//  Render
// ─────────────────────────────────────────
void render() {
    SDL_SetRenderDrawColor(gRenderer,0,0,0,255);
    SDL_RenderClear(gRenderer);

    // ── SHOP ─────────────────────────────────────────────────────────────
    if (G.state==S_SHOP) {
        fillRect(0,0,WIN_W,WIN_H,18,18,32);
        drawText("UPGRADE SHOP",120,35,{255,215,0,255},26);
        std::ostringstream cs; cs<<"Coins: "<<G.coins;
        drawText(cs.str(),165,72,{255,215,0,255},17);

        const char* names[3]={"DRILL SPEED","BATTERY DEPTH","BAG CAPACITY"};
        const char* descs[3]={"Drill drops faster","Reach deeper depths","Carry more ores per run"};
        int   base[3]={50,80,60};
        int*  lvls[3]={&G.speedLvl,&G.batteryLvl,&G.bagLvl};
        SDL_Color cols[3]={{100,200,255,255},{100,255,150,255},{255,180,100,255}};

        for (int i=0;i<3;i++) {
            int bx=28,by=108+i*168;
            int lvl=*lvls[i],cost=base[i]*lvl*lvl;
            fillRect(bx,by,WIN_W-56,150,28,28,48);
            outlineRect(bx,by,WIN_W-56,150,cols[i]);
            drawText(names[i],bx+12,by+10,cols[i],17);
            drawText(descs[i],bx+12,by+33,{175,175,175,255},13);
            for (int l=0;l<5;l++) {
                SDL_Color pc=(l<lvl)?cols[i]:SDL_Color{48,48,68,255};
                fillRect(bx+12+l*46,by+57,40,14,pc);
                outlineRect(bx+12+l*46,by+57,40,14,0,0,0);
            }
            std::ostringstream ls; ls<<"Level "<<lvl<<" / 5";
            drawText(ls.str(),bx+255,by+55,{185,185,185,255},13);
            bool ok=(G.coins>=cost&&lvl<5);
            std::ostringstream btnS;
            if(lvl>=5) btnS<<"MAX LEVEL"; else btnS<<"Upgrade ("<<cost<<" coins)";
            drawButton(bx+12,by+86,215,36,
                       ok?42:62, ok?125:62, ok?42:62,
                       175,175,175,
                       btnS.str());
        }
        drawButton(10,8,90,30, 72,32,32, 185,85,85,"< Back",{255,185,185,255},13);
        SDL_RenderPresent(gRenderer);
        return;
    }

    // ── GAME ─────────────────────────────────────────────────────────────
    fillRect(0,0,WIN_W,GROUND_TOP,62,115,165);
    fillRect(0,GROUND_TOP-18,WIN_W,18,48,90,128);

    int startRow=std::max(0,(int)(G.camY/TILE));
    int endRow=std::min(startRow+WIN_H/TILE+2,WORLD_ROWS);
    for (int r=startRow;r<endRow;r++) {
        for (int c=0;c<WORLD_COLS;c++) {
            RockType rt=world[r][c];
            int px=c*TILE, py=(int)(GROUND_TOP+r*TILE-G.camY);
            if (rt==EMPTY){ fillRect(px,py,TILE-1,TILE-1,8,4,2); continue; }
            fillRect(px,py,TILE-1,TILE-1,ROCKS[rt].color);
            outlineRect(px,py,TILE-1,TILE-1,0,0,0);
        }
    }

    if (G.state==S_SURFACE) {
        int col=std::max(0,std::min(WORLD_COLS-1,(int)(G.minerX/TILE)));
        int px=col*TILE;
        for (int r2=0;r2<5;r2++) {
            int py=GROUND_TOP+r2*TILE-(int)G.camY;
            fillRect(px,py,TILE-1,TILE-1,80,80,10);
            outlineRect(px,py,TILE-1,TILE-1,220,200,0);
        }
        int ax=px+TILE/2-4;
        drawText("v",ax,GROUND_TOP+4,{255,240,0,255},16);
    }

    fillRect(0,SURFACE_Y+26,WIN_W,24,72,50,26);
    outlineRect(0,SURFACE_Y+26,WIN_W,24,52,36,16);

    if (G.state==S_SURFACE||G.state==S_RETURNING) {
        int mx=(int)G.minerX;
        fillRect(mx-8, SURFACE_Y-6,  16,30,70,130,200);
        fillRect(mx-6, SURFACE_Y-26, 12,16,255,200,150);
        fillRect(mx-9, SURFACE_Y-32, 20, 9,255,195,40);
        fillRect(mx+5, SURFACE_Y-32,  7, 7,255,255,80);
        fillRect(mx+7, SURFACE_Y-2,   6,12,255,200,150);
    }

    if (G.state==S_DRILLING||G.state==S_RETURNING) {
        int drillScreenY=(int)(G.drillY-G.camY);
        int cx=(int)G.drillX;
        if (drillScreenY>SURFACE_Y+26)
            fillRect(cx-2,SURFACE_Y+26,4,drillScreenY-SURFACE_Y-26,82,48,16);

        int dx=cx-14, dy=drillScreenY-22;
        fillRect(dx,   dy,   28,30,178,178,190);
        outlineRect(dx,dy,   28,30,65,65,65);
        fillRect(dx+4, dy+6, 20, 4,108,108,120);
        fillRect(dx+4, dy+14,20, 4,108,108,120);
        fillRect(dx+2, dy+30,24, 6,255,215,0);
        fillRect(dx+5, dy+36,18, 5,240,198,0);
        fillRect(dx+9, dy+41,10, 5,218,178,0);
        fillRect(dx+12,dy+46, 4, 4,198,158,0);
    }

    for (auto& p:G.particles)
        fillRect((int)p.x-3,(int)p.y-3,6,6,p.col);

    // ── HUD ───────────────────────────────────────────────────────────────
    fillRect(4,4,150,28,0,0,0);
    outlineRect(4,4,150,28,255,215,0);
    std::ostringstream cs; cs<<"Coins: "<<G.coins;
    drawText(cs.str(),10,8,{255,215,0,255},16);

    fillRect(4,36,150,24,0,0,0);
    outlineRect(4,36,150,24,135,135,135);
    std::ostringstream bs; bs<<"Bag: "<<G.bag.size()<<"/"<<G.getBagCap();
    drawText(bs.str(),10,39,{198,198,198,255},14);

    if (G.state==S_DRILLING||G.state==S_RETURNING) {
        fillRect(WIN_W-168,4,164,28,0,0,0);
        outlineRect(WIN_W-168,4,164,28,95,175,255);
        std::ostringstream ds;
        ds<<"Depth: "<<(int)G.depthM<<"m / "<<G.getMaxDepth()<<"m";
        drawText(ds.str(),WIN_W-163,8,{145,215,255,255},13);

        int barH=WIN_H-GROUND_TOP;
        fillRect(WIN_W-20,GROUND_TOP,16,barH,26,26,26);
        float pct=std::min(1.f,G.depthM/G.getMaxDepth());
        fillRect(WIN_W-20,GROUND_TOP,16,(int)(barH*pct),75,155,255);
        outlineRect(WIN_W-20,GROUND_TOP,16,barH,52,52,52);
    }

    int showN=std::min((int)G.bag.size(),7);
    for (int i=0;i<showN;i++) {
        auto& ore=G.bag[G.bag.size()-1-i];
        int oy=WIN_H-26-i*22;
        fillRect(4,oy,148,19,0,0,0);
        fillRect(7,oy+3,12,12,ROCKS[ore.type].color);
        std::ostringstream os; os<<ROCKS[ore.type].name<<" +"<<ore.value;
        drawText(os.str(),24,oy+2,{228,228,228,255},13);
    }

    if (G.state==S_SURFACE) {
        drawText("A/D or Arrow Keys to move  |  Click ground to drill",
                 6,WIN_H-20,{148,148,148,255},11);
        int bx=(int)G.minerX-80, by=SURFACE_Y-70;
        bx=std::max(2,std::min(WIN_W-162,bx));
        drawButton(bx,by,160,36, 35,120,35, 85,205,85,"  SEND DRILL DOWN");
        drawButton(WIN_W-152,4,148,28, 82,62,8, 255,215,0,"  UPGRADE SHOP",{255,215,0,255},12);
    }

    if (G.state==S_DRILLING) {
        fillRect(WIN_W/2-118,WIN_H-34,236,26,0,0,0);
        drawText("Click to recall drill early",WIN_W/2-104,WIN_H-30,{175,175,175,255},14);
    }

    SDL_RenderPresent(gRenderer);
}

// ─────────────────────────────────────────
//  Main loop
// ─────────────────────────────────────────
void mainLoop() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch(e.type) {
        case SDL_QUIT:
#ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
#else
            SDL_Quit(); exit(0);
#endif
            break;

        case SDL_KEYDOWN:
            if (G.state==S_SURFACE) {
                if (e.key.keysym.sym==SDLK_LEFT  || e.key.keysym.sym==SDLK_a) G.movingLeft=true;
                if (e.key.keysym.sym==SDLK_RIGHT || e.key.keysym.sym==SDLK_d) G.movingRight=true;
                if (e.key.keysym.sym==SDLK_SPACE || e.key.keysym.sym==SDLK_RETURN) G.startDrill();
            }
            break;

        case SDL_KEYUP:
            if (e.key.keysym.sym==SDLK_LEFT  || e.key.keysym.sym==SDLK_a) G.movingLeft=false;
            if (e.key.keysym.sym==SDLK_RIGHT || e.key.keysym.sym==SDLK_d) G.movingRight=false;
            break;

        case SDL_MOUSEBUTTONDOWN: {
            int mx=e.button.x, my=e.button.y;
            if (G.state==S_SURFACE) {
                if (inRect(mx,my,WIN_W-152,4,148,28)) { G.state=S_SHOP; break; }
                int bx=(int)G.minerX-80, by=SURFACE_Y-70;
                bx=std::max(2,std::min(WIN_W-162,bx));
                if (inRect(mx,my,bx,by,160,36)) { G.startDrill(); break; }
                if (my>=GROUND_TOP) {
                    int col=std::max(0,std::min(WORLD_COLS-1,mx/TILE));
                    G.minerX=(float)(col*TILE+TILE/2);
                    G.startDrill(); break;
                }
                if (my>=SURFACE_Y-40) G.minerX=(float)mx;
            }
            else if (G.state==S_DRILLING) G.state=S_RETURNING;
            else if (G.state==S_SHOP) {
                if (inRect(mx,my,10,8,90,30)) { G.state=S_SURFACE; break; }
                int base[3]={50,80,60};
                int* lvls[3]={&G.speedLvl,&G.batteryLvl,&G.bagLvl};
                for (int i=0;i<3;i++) {
                    int bx2=28+12, by2=108+i*168+86;
                    if (inRect(mx,my,bx2,by2,215,36)) {
                        int lvl=*lvls[i], cost=base[i]*lvl*lvl;
                        if (lvl<5&&G.coins>=cost) { G.coins-=cost; (*lvls[i])++; }
                    }
                }
            }
            break;
        }
        }
    }

    G.update(G.getDt());
    render();
}

// ─────────────────────────────────────────
//  Entry point
// ─────────────────────────────────────────
int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    gWindow   = SDL_CreateWindow("Hard Working Miner",
                    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                    WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    gRenderer = SDL_CreateRenderer(gWindow, -1,
                    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    // Load a font — tries a few common paths for Windows / Linux / Emscripten
    const char* fontPaths[] = {
        "arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        nullptr
    };
    TTF_Font* baseFont = nullptr;
    for (int i=0; fontPaths[i]; i++) {
        baseFont = TTF_OpenFont(fontPaths[i], 16);
        if (baseFont) break;
    }
    if (!baseFont) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Font Error",
            "Could not load a font. Place arial.ttf next to the exe, or install DejaVu fonts.",
            gWindow);
        return 1;
    }

    // Open the same file at each needed size
    const char* fontPath = fontPaths[0];
    // Re-find which path worked
    for (int i=0; fontPaths[i]; i++) {
        TTF_Font* t = TTF_OpenFont(fontPaths[i],16);
        if (t) { TTF_CloseFont(t); fontPath=fontPaths[i]; break; }
    }
    TTF_CloseFont(baseFont);

    gFont11 = TTF_OpenFont(fontPath,11);
    gFont12 = TTF_OpenFont(fontPath,12);
    gFont13 = TTF_OpenFont(fontPath,13);
    gFont14 = TTF_OpenFont(fontPath,14);
    gFont16 = TTF_OpenFont(fontPath,16);
    gFont17 = TTF_OpenFont(fontPath,17);
    gFont26 = TTF_OpenFont(fontPath,26);

    G.init();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
    while (true) {
        mainLoop();
        SDL_Delay(16); // ~60fps
    }
#endif

    TTF_CloseFont(gFont11); TTF_CloseFont(gFont12);
    TTF_CloseFont(gFont13); TTF_CloseFont(gFont14);
    TTF_CloseFont(gFont16); TTF_CloseFont(gFont17);
    TTF_CloseFont(gFont26);
    TTF_Quit();
    SDL_DestroyRenderer(gRenderer);
    SDL_DestroyWindow(gWindow);
    SDL_Quit();
    return 0;
}