// Deterministic 640x480 review of the production renderer, atlas and upgrade UI.
// c++ -std=c++17 -Isrc $(sdl2-config --cflags) tools/render_art_review.cpp
//     -o build/art-review $(sdl2-config --libs)
#include "render.hpp"
#include <iostream>
int main() {
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 1;
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer *r = SDL_CreateSoftwareRenderer(surface);
    if (!r)
        return 1;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    {
        nf::World w;
        w.reset();
        w.time = 245;
        for (int i = 0; i < 4; ++i) {
            w.players[i].active = true;
            w.players[i].pos = {800.f + i * 46.f, 800.f};
            w.players[i].level = 6;
            w.players[i].pending = 1;
        }
        for (int i = 0; i < 4; ++i)
            w.enemies.push_back({{640.f + i * 78.f, 710.f}, 100, 0, i, uint32_t(i + 1)});
        for (int i = 0; i < 24; ++i)
            w.enemies.push_back({{650.f + (i % 8) * 20.f, 880.f + (i / 8) * 21.f},
                                 100, 0, i % 3, uint32_t(i + 5)});
        nf::Paint paint(r, {});
        paint.world(w, 0);
        SDL_RenderPresent(r);
        SDL_SaveBMP(surface, "build/art-review-world.bmp");
        // Two sets cover the complete upgrade icon family and both selection styles.
        for (int page = 0; page < 2; ++page) {
            w.players[0].level = page ? 5 : 6;
            paint.world(w, 0);
            paint.upgrades(w.players[0], page, false);
            SDL_RenderPresent(r);
            SDL_SaveBMP(surface, page ? "build/art-review-upgrades-b.bmp" : "build/art-review-upgrades-a.bmp");
        }
    }
    SDL_DestroyRenderer(r);
    SDL_FreeSurface(surface);
    SDL_Quit();
    std::cout << "Rendered world, four teams, all enemies and six upgrade icons at 640x480\n";
}
