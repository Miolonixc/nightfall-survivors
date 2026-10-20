#pragma once
#include "sprite_data.hpp"
#include <SDL.h>
#include <stdexcept>
namespace nf {
// One immutable nearest-neighbour texture, uploaded once for all art and animation.
struct Sprites {
    SDL_Renderer *renderer;
    SDL_Texture *texture = nullptr;
    explicit Sprites(SDL_Renderer *r) : renderer(r) {
        SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, ArtWidth, ArtHeight, 32,
                                                       SDL_PIXELFORMAT_ARGB8888);
        if (!s)
            throw std::runtime_error(SDL_GetError());
        int at = 0;
        for (size_t k = 0; k < sizeof(ArtRle); k += 2)
            for (int n = 0; n < ArtRle[k]; ++n, ++at)
                reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(s->pixels) +
                                              (at / ArtWidth) * s->pitch)[at % ArtWidth] =
                    ArtPalette[ArtRle[k + 1]];
        texture = SDL_CreateTextureFromSurface(renderer, s);
        SDL_FreeSurface(s);
        if (!texture)
            throw std::runtime_error(SDL_GetError());
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }
    Sprites(const Sprites &) = delete;
    Sprites &operator=(const Sprites &) = delete;
    ~Sprites() { SDL_DestroyTexture(texture); }
    void draw(int id, int x, int y, int scale = 2, uint8_t alpha = 255) {
        const auto &f = ArtFrames[id];
        SDL_Rect src{f.x, f.y, f.w, f.h};
        SDL_Rect dst{x + f.ox * scale, y + f.oy * scale, f.w * scale, f.h * scale};
        if (dst.x > 640 || dst.x + dst.w < 0 || dst.y > 480 || dst.y + dst.h < 0)
            return;
        SDL_SetTextureAlphaMod(texture, alpha);
        SDL_RenderCopy(renderer, texture, &src, &dst);
    }
};
} // namespace nf
