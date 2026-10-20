#pragma once
#include "font_data.hpp"
#include <SDL.h>
#include <array>
#include <string>
namespace nf {
struct Fonts {
    SDL_Renderer *renderer;
    std::array<SDL_Texture *, 4> textures{};
    explicit Fonts(SDL_Renderer *r) : renderer(r) {
        for (size_t i = 0; i < textures.size(); ++i) {
            const auto &f = FontFaces[i];
            SDL_Surface *s =
                SDL_CreateRGBSurfaceWithFormat(0, f.w, f.h, 32, SDL_PIXELFORMAT_ARGB8888);
            if (!s)
                continue;
            size_t at = 0;
            for (int k = 0; k < f.rleSize; k += 2)
                for (int n = 0; n < f.rle[k]; ++n) {
                    int y = int(at) / f.w, x = int(at) % f.w;
                    reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(s->pixels) +
                                                 y * s->pitch)[x] =
                        0x00ffffffu | (uint32_t(f.rle[k + 1]) << 24);
                    ++at;
                }
            textures[i] = SDL_CreateTextureFromSurface(renderer, s);
            SDL_FreeSurface(s);
            if (textures[i])
                SDL_SetTextureBlendMode(textures[i], SDL_BLENDMODE_BLEND);
        }
    }
    ~Fonts() {
        for (auto *t : textures)
            if (t)
                SDL_DestroyTexture(t);
    }
    int index(int scale) const { return scale >= 5 ? 3 : (scale >= 3 ? 2 : (scale >= 2 ? 1 : 0)); }
    int width(const std::string &s, int scale) const {
        const auto &f = FontFaces[index(scale)];
        int w = 0, line = 0;
        for (unsigned char c : s) {
            if (c == '\n') {
                w = std::max(w, line);
                line = 0;
            } else if (c >= 32 && c < 127)
                line += f.advances[c - 32];
        }
        return std::max(w, line);
    }
    void draw(int x, int y, const std::string &s, SDL_Color color, int scale) {
        int i = index(scale);
        const auto &f = FontFaces[i];
        SDL_Texture *t = textures[i];
        if (!t)
            return;
        SDL_SetTextureColorMod(t, color.r, color.g, color.b);
        SDL_SetTextureAlphaMod(t, color.a);
        int start = x;
        for (unsigned char c : s) {
            if (c == '\n') {
                y += f.lineHeight;
                x = start;
                continue;
            }
            if (c < 32 || c >= 127)
                continue;
            int glyph = c - 32;
            if (c != ' ') {
                SDL_Rect src{(glyph % f.cols) * f.cellW, (glyph / f.cols) * f.cellH, f.cellW,
                             f.cellH};
                SDL_Rect dst{x + f.offsetX, y - 2, f.cellW, f.cellH};
                SDL_RenderCopy(renderer, t, &src, &dst);
            }
            x += f.advances[glyph];
        }
    }
};
} // namespace nf
