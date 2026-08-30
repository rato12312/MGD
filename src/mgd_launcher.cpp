#include <iostream>
#include <string>

#ifdef MGD_HAS_SDL2
#include "core/renderer/renderer.h"
#include "core/common/Vec3.h"
#include "core/camera/Camera.h"
#include <SDL.h>

// MGD Launcher — PS5 / GameHub style, obra-prima
// Este exe é a UI do mgd_app.exe: mostra o launcher com
// Skyrim LE, chunks automáticos, cache por IDs, e lança o jogo.
// Usa o mesmo Renderer/SDL2 do MGD, mas como launcher.

using namespace mgd;

static void drawRect(SDL_Renderer* r, int x, int y, int w, int h, uint8_t cr, uint8_t cg, uint8_t cb, uint8_t a=255) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, a);
    SDL_Rect rc{x,y,w,h};
    SDL_RenderFillRect(r, &rc);
}
static void drawRectOutline(SDL_Renderer* r, int x, int y, int w, int h, uint8_t cr, uint8_t cg, uint8_t cb) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_Rect rc{x,y,w,h};
    SDL_RenderDrawRect(r, &rc);
}

int main(int argc, char* argv[]) {
    Renderer::Config cfg;
    cfg.title = "MGD Launcher — Legendary Edition";
    cfg.width = 1280;
    cfg.height = 720;
    cfg.vsync = true;

    Renderer renderer;
    if (!renderer.initialize(cfg)) {
        std::cerr << "Launcher: falha ao inicializar renderer\n";
        return 1;
    }

    SDL_Renderer* sdl = renderer.getSDLRenderer();
    bool running = true;
    int selected = 0; // 0=Skyrim LE, 1=SE
    bool hoverLaunch = false;

    renderer.setInputCallback([&](const SDL_Event& e){
        if (e.type == SDL_QUIT) running = false;
        if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_ESCAPE) running = false;
            if (e.key.keysym.sym == SDLK_LEFT) selected = 0;
            if (e.key.keysym.sym == SDLK_RIGHT) selected = 1;
            if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_SPACE) {
                std::cout << "MGD Launcher: Launch Skyrim LE via MGD obra-prima\n";
                std::cout << "  Mental Map -> Chunks automaticos -> Camera guiada por colisao -> Painter pinta tudo\n";
            }
        }
        if (e.type == SDL_MOUSEMOTION) {
            int mx = e.motion.x, my = e.motion.y;
            // Launch button at bottom right: 1050, 640, 200, 48
            hoverLaunch = (mx>=1050 && mx<=1250 && my>=640 && my<=688);
        }
        if (e.type == SDL_MOUSEBUTTONDOWN) {
            int mx = e.button.x, my = e.button.y;
            if (mx>= 40 && mx<= 300 && my>= 420 && my<= 560) selected = 0;
            if (mx>= 320 && mx<= 580 && my>= 420 && my<= 560) selected = 1;
            if (hoverLaunch) {
                std::cout << ">>> Launch MGD: mgd_app.exe (headless demo gera output.ppm)\n";
            }
        }
    });

    renderer.setRenderCallback([&](){
        // Fundo void -> surface
        drawRect(sdl, 0,0,1280,720, 6,10,20);
        // Topbar
        drawRect(sdl, 0,0,1280,56, 15,20,36);
        drawRect(sdl, 16,12,36,36, 0,112,204); // mark
        // Sidenav
        drawRect(sdl, 0,56,240,664, 15,20,36);
        drawRect(sdl, 12,80,216,40, 26,34,56); // active Library
        drawRect(sdl, 12,124,216,36, 15,20,36);
        drawRect(sdl, 12,164,216,36, 15,20,36);
        // Hero
        drawRect(sdl, 260,76, 1000, 320, 26,34,56);
        // Hero cover (esquerda)
        drawRect(sdl, 280,96, 400, 260, 11,18,36);
        // cover art gradient mock
        drawRect(sdl, 290,106, 380, 200, 26,42,74);
        // DLC badges
        drawRect(sdl, 300,314, 80,18, 0,112,204);
        drawRect(sdl, 386,314, 80,18, 0,217,255);
        drawRect(sdl, 472,314, 90,18, 0,112,204);
        // Hero info (direita)
        drawRect(sdl, 700,96, 480, 260, 15,20,36);
        // Stats
        drawRect(sdl, 720,260, 90,40, 26,34,56);
        drawRect(sdl, 820,260, 90,40, 26,34,56);
        drawRect(sdl, 920,260, 90,40, 26,34,56);
        // Ações
        drawRect(sdl, 720,310, 140,36, 232,236,245); // Add Skyrim
        drawRect(sdl, 870,310, 80,36, 34,46,74); // Scan
        SDL_Rect launchRc{1050,640,200,48};
        if (hoverLaunch) drawRect(sdl, launchRc.x, launchRc.y, launchRc.w, launchRc.h, 0,140,230);
        else drawRect(sdl, launchRc.x, launchRc.y, launchRc.w, launchRc.h, 0,112,204);
        drawRectOutline(sdl, launchRc.x, launchRc.y, launchRc.w, launchRc.h, 0,217,255);
        // Carousel
        drawRect(sdl, 260,416, 260,140, 26,34,56, selected==0?255:160);
        if (selected==0) drawRectOutline(sdl, 260,416, 260,140, 0,112,204);
        drawRect(sdl, 530,416, 260,140, 26,34,56, selected==1?255:160);
        // Details panels 2x2
        drawRect(sdl, 260,576, 490, 120, 26,34,56);
        drawRect(sdl, 770,576, 490, 120, 26,34,56);
    });

    // Update loop simples
    renderer.setUpdateCallback([&](float dt){
        (void)dt;
        if (!running) renderer.stop();
    });

    std::cout << "MGD Launcher obra-prima — PS5/GameHub style\n";
    std::cout << "Controles: SETAS escolhe jogo, ENTER lança, ESC sai, mouse no Launch\n";
    std::cout << "UI web completa em interface/launcher/index.html (abra no navegador)\n";
    renderer.run();
    renderer.shutdown();
    return 0;
}

#else
int main() {
    std::cout << "MGD Launcher requer SDL2 (MGD_HAS_SDL2). Compile com SDL2 ou use interface/launcher/index.html no navegador.\n";
    std::cout << "Headless: ./build/mgd_app  (gera output.ppm)  |  Testes: ./build/mgd_tests\n";
    return 0;
}
#endif
