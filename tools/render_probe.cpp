// Hardware diagnostic: separate input pumping, drawing and display waits.
#include "render.hpp"
#include <chrono>
#include <iostream>
#include <thread>
int main(int argc, char **) {
    using Clock = std::chrono::steady_clock;
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_Window *w = SDL_CreateWindow("Nightfall render probe", 0, 0, 640, 480, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_Renderer *r = SDL_CreateRenderer(w, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!r) return 1;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_GameController *pad = argc > 1 ? SDL_GameControllerOpen(0) : nullptr;
    SDL_RendererInfo info{}; SDL_GetRendererInfo(r, &info);
    double poll=0, draw=0, present=0, delay=0;
    {
        nf::Paint paint(r, {});
        nf::World world; world.reset();
        for (int i=0; i<240; ++i)
            world.enemies.push_back({{650.f+(i%16)*20, 650.f+(i/16)*20}, 100, 0, i%3, uint32_t(i)});
        for (int frame=0;frame<180;++frame) {
            auto a=Clock::now();
            SDL_Event event; while (SDL_PollEvent(&event)) {}
            auto b=Clock::now();
            world.time=frame/60.f;
            paint.world(world,0);
            auto c=Clock::now();
            SDL_RenderPresent(r);
            auto d=Clock::now();
            if (argc > 2 && !(info.flags & SDL_RENDERER_PRESENTVSYNC))
                std::this_thread::sleep_until(a + std::chrono::microseconds(16667));
            delay+=std::chrono::duration<double,std::milli>(Clock::now()-d).count();
            poll+=std::chrono::duration<double,std::milli>(b-a).count();
            draw+=std::chrono::duration<double,std::milli>(c-b).count();
            present+=std::chrono::duration<double,std::milli>(d-c).count();
        }
    }
    std::cout << "renderer_flags=" << info.flags << " controller=" << bool(pad) << " poll_ms=" << poll/180 << " draw_ms=" << draw/180 << " present_ms=" << present/180 << " delay_ms=" << delay/180 << " fps=" << 180000/(poll+draw+present+delay) << '\n';
    if (pad) SDL_GameControllerClose(pad);
    SDL_DestroyRenderer(r);SDL_DestroyWindow(w);SDL_Quit();
}
