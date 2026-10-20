#pragma once
#include "fonts.hpp"
#include "network.hpp"
#include "sprites.hpp"
#include <SDL.h>
#include <iomanip>
#include <map>
#include <sstream>
namespace nf {
struct Color {
    uint8_t r, g, b, a = 255;
};
constexpr Color Ink{12, 19, 29}, Panel{21, 32, 45}, Muted{128, 153, 168}, White{229, 239, 232},
    Gold{251, 194, 89}, Teal{87, 227, 194}, Red{246, 104, 123};
inline Color playerColor(int i) {
    static Color c[] = {Teal, {170, 139, 255}, {255, 189, 91}, {111, 188, 255}};
    return c[i % 4];
}
struct Paint {
    SDL_Renderer *r;
    Vec camera;
    Fonts fonts;
    Sprites sprites;
    std::array<Vec, 4> previousHero{};
    Paint(SDL_Renderer *renderer, Vec origin)
        : r(renderer), camera(origin), fonts(renderer), sprites(renderer) {}
    void color(Color c) { SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a); }
    void box(int x, int y, int w, int h, Color c) {
        color(c);
        SDL_Rect a{x, y, w, h};
        SDL_RenderFillRect(r, &a);
    }
    // Two-pixel ink borders preserve silhouettes where sprites overlap at 640x480.
    void spriteBox(int x, int y, int w, int h, Color c) {
        box(x - 2, y - 2, w + 4, h + 4, {7, 12, 20});
        box(x, y, w, h, c);
    }
    void frame(int x, int y, int w, int h, Color c) {
        color(c);
        SDL_Rect a{x, y, w, h};
        SDL_RenderDrawRect(r, &a);
    }
    void line(int x, int y, int xx, int yy, Color c) {
        color(c);
        SDL_RenderDrawLine(r, x, y, xx, yy);
    }
    void circle(int x, int y, int radius, Color c) {
        color(c);
        for (int yy = -radius; yy <= radius; yy++) {
            int span = int(std::sqrt(float(radius * radius - yy * yy)));
            SDL_RenderDrawLine(r, x - span, y + yy, x + span, y + yy);
        }
    }
    void diamond(int x, int y, int rad, Color c) {
        for (int yy = -rad; yy <= rad; yy++)
            box(x - (rad - std::abs(yy)), y + yy, 2 * (rad - std::abs(yy)) + 1, 1, c);
    }
    void text(int x, int y, const std::string &s, Color c = White, int scale = 2) {
        fonts.draw(x, y, s, {c.r, c.g, c.b, c.a}, scale);
    }
    void center(int y, const std::string &s, Color c = White, int scale = 2) {
        text((640 - fonts.width(s, scale)) / 2, y, s, c, scale);
    }
    Vec screen(Vec p) { return p - camera + Vec{320, 240}; }
    void person(Vec pos, int id, float time, bool down = false, bool hurt = false) {
        Vec s = screen(pos);
        int x = int(s.x), y = int(s.y) + 12;
        sprites.draw(Shadow, x, y);
        if (down) {
            sprites.draw(Bones, x, y);
            diamond(x, y - 14, 7, Ink);
            box(x - 2, y - 20, 4, 13, White);
            box(x - 6, y - 16, 12, 4, White);
            return;
        }
        bool moving = length(pos - previousHero[id]) > .1f;
        previousHero[id] = pos;
        int frame = moving ? int(time * 9) % 4 : 0;
        // A subtle foot rune and pale sprite rim keep every team visible in a crowd.
        line(x - 13, y + 2, x - 7, y + 5, playerColor(id));
        line(x + 7, y + 5, x + 13, y + 2, playerColor(id));
        sprites.draw(Wizard0_0 + id * 4 + frame, x, y);
        if (hurt && int(time * 12) % 2) {
            line(x - 7, y - 49, x, y - 53, White);
            line(x, y - 53, x + 7, y - 49, White);
        }
    }
    void monster(const Enemy &e, float t) {
        Vec s = screen(e.pos);
        int x = int(s.x), y = int(s.y) + 12;
        if (x < -65 || x > 705 || y < -20 || y > 560)
            return;
        int frame = int(t * (e.type == 1 ? 10 : 6) + e.id) % 4;
        int base = e.type == 3 ? Warden0 : e.type == 2 ? Guard0 : e.type == 1 ? Bat0 : Ghoul0;
        sprites.draw(base + frame, x, y, e.type == 3 ? 3 : 2);
        if (e.flash > 0) {
            int top = e.type == 1 ? 23 : e.type == 3 ? 72 : 37;
            line(x - 4, y - top, x, y - top - 4, White);
            line(x, y - top - 4, x + 4, y - top, Gold);
        }
    }
    void world(const World &w, int local) {
        int id = std::clamp(local, 0, 3);
        camera = w.players[id].pos;
        camera.x = std::clamp(camera.x, 320.f, Arena - 320);
        camera.y = std::clamp(camera.y, 240.f, Arena - 240);
        box(0, 0, 640, 480, {17, 27, 34});
        // A broken processional path guides the eye without a high-contrast tile grid.
        for (int yy = int(camera.y - 280) / 48; yy < (camera.y + 280) / 48; yy++)
            for (int xx = int(camera.x - 370) / 48; xx < (camera.x + 370) / 48; xx++) {
                uint32_t h = (uint32_t(xx) * 73856093u) ^ (uint32_t(yy) * 19349663u);
                Vec s = screen({xx * 48.f, yy * 48.f});
                int x = int(s.x), y = int(s.y);
                box(x, y, 48, 48, h % 3 ? Color{19, 30, 36} : Color{21, 33, 38});
                // Sparse subdued soil flecks, with no animated ground noise.
                box(x + int(h % 37), y + int((h >> 5) % 39), 2, 1, {29, 40, 42});
                bool path = std::abs(xx * 48 - 800) < 60 || std::abs(yy * 48 - 800) < 45;
                if (path) {
                    box(x + 4, y + 5, 40, 37, {25, 37, 43});
                    line(x + 8, y + 5, x + 39, y + 5, {34, 45, 49});
                    line(x + 4, y + 8, x + 4, y + 37, {30, 43, 47});
                    if (h % 4 == 0)
                        line(x + 21, y + 9, x + 18, y + 18, {16, 26, 33});
                } else if (h % 7 == 0) {
                    sprites.draw(Grass, x + 24, y + 38);
                }
                if (!path && h % 19 == 0) {
                    int prop = h % 3 == 0 ? Cross : h % 3 == 1 ? Grave : Obelisk;
                    sprites.draw(prop, x + 24, y + 36);
                }
                if (!path && h % 43 == 0)
                    sprites.draw(Bones, x + 20, y + 32);
                if (h % 47 == 0) {
                    // Warm light is confined to the ground, below all combat silhouettes.
                    sprites.draw(EmberGlow, x + 24, y + 65, 3, 200);
                    sprites.draw(Brazier0 + int(w.time * 7 + h % 4) % 4, x + 24, y + 36);
                }
            }
        // Ground shadows form a separate pass so they never darken another enemy's face.
        for (const auto &e : w.enemies) {
            Vec s = screen(e.pos);
            sprites.draw(Shadow, int(s.x), int(s.y) + 12, e.type == 3 ? 3 : 2,
                         e.type == 1 ? 110 : 220);
        }
        for (auto &g : w.gems) {
            Vec s = screen(g.pos);
            diamond(int(s.x), int(s.y), 4, Ink);
            diamond(int(s.x), int(s.y), 3, g.value > 1 ? Gold : Color{62, 145, 132});
            box(int(s.x), int(s.y) - 2, 1, 2, White);
        }
        // Stable foot ordering prevents spawn order from cutting faces with distant bodies.
        std::vector<const Enemy *> depth;
        depth.reserve(w.enemies.size());
        for (const auto &e : w.enemies)
            depth.push_back(&e);
        std::sort(depth.begin(), depth.end(), [](const Enemy *a, const Enemy *b) {
            return a->pos.y == b->pos.y ? a->id < b->id : a->pos.y < b->pos.y;
        });
        for (const auto *e : depth)
            monster(*e, w.time);
        for (auto &s : w.shots) {
            Vec p = screen(s.pos);
            Vec trail = p - unit(s.vel) * 14;
            line(int(trail.x), int(trail.y), int(p.x), int(p.y), playerColor(s.owner));
            diamond(int(p.x), int(p.y), 5, Ink);
            diamond(int(p.x), int(p.y), 4, playerColor(s.owner));
            diamond(int(p.x), int(p.y), 2, White);
        }
        for (auto &s : w.sparks) {
            Vec p = screen(s.pos);
            int rad = int((.35f - s.life) * 42);
            for (int k = 0; k < 6; k++) {
                float a = k * Pi / 3;
                box(int(p.x + std::cos(a) * rad), int(p.y + std::sin(a) * rad), 3, 3,
                    s.kind == 0 ? Red : Gold);
            }
            if (s.life > .22f) {
                line(int(p.x) - 3, int(p.y), int(p.x) + 3, int(p.y), White);
                line(int(p.x), int(p.y) - 3, int(p.x), int(p.y) + 3, White);
            }
        }
        for (int i = 0; i < 4; i++) {
            auto &p = w.players[i];
            if (!p.active)
                continue;
            if (p.upgrades[3] && p.hp > 0) {
                for (int k = 0; k < 6; k++) {
                    float a = w.time * 2 + k * Pi / 3;
                    Vec s =
                        screen(p.pos + Vec{std::cos(a), std::sin(a)} * (48 + 6 * p.upgrades[3]));
                    diamond(int(s.x), int(s.y), 5, Ink);
                    diamond(int(s.x), int(s.y), 4, {75, 135, 174});
                    line(int(s.x), int(s.y) - 3, int(s.x), int(s.y) + 2, {185, 224, 238});
                }
            }
            person(p.pos, i, w.time, p.hp <= 0, p.invuln > 0);
            Vec s = screen(p.pos);
            text(int(s.x) - 10, int(s.y) - 54, "P" + std::to_string(i + 1), playerColor(i), 1);
            if (p.hp <= 0) {
                box(int(s.x) - 20, int(s.y) + 24, 40, 4, Panel);
                box(int(s.x) - 20, int(s.y) + 24, int(40 * p.revive / 3), 4, Teal);
            }
        }
        box(0, 0, 640, 53, Ink);
        box(0, 51, 640, 2, {47, 54, 58});
        line(12, 53, 143, 53, {99, 84, 57});
        line(497, 53, 628, 53, {99, 84, 57});
        diamond(8, 52, 3, Gold);
        diamond(632, 52, 3, Gold);
        text(16, 12, "NIGHTFALL", Teal, 2);
        auto &p = w.players[id];
        frame(155, 10, 145, 16, {122, 96, 64});
        box(157, 12, 141, 12, {48, 30, 42});
        box(157, 12, int(141 * std::clamp(p.hp / maxHp(p), 0.f, 1.f)), 12, Red);
        int hpWidth = int(141 * std::clamp(p.hp / maxHp(p), 0.f, 1.f));
        box(157, 12, hpWidth, 3, {255, 146, 150});
        for (int mark = 1; mark < 4; ++mark)
            box(157 + mark * 35, 13, 1, 10, {72, 40, 49});
        text(157, 32, "HP " + std::to_string(int(p.hp)) + "/" + std::to_string(int(maxHp(p))),
             Muted, 1);
        int secs = int(w.time);
        char clock[24];
        std::snprintf(clock, sizeof clock, "%02d:%02d", secs / 60, secs % 60);
        text(329, 13, clock, Gold, 3);
        text(453, 14, "LV " + std::to_string(p.level), White, 2);
        text(453, 35, "KILLS " + std::to_string(p.kills), Muted, 1);
        text(568, 15, "P" + std::to_string(id + 1), playerColor(id), 2);
        box(0, 436, 640, 44, Ink);
        box(0, 436, 640, 4, {38, 59, 66});
        box(0, 436, int(640.f * p.xp / xpNeeded(p.level)), 4, Teal);
        text(15, 451,
             w.bossKilled ? "WARDEN DEFEATED"
                          : (w.time >= 240 ? "DEFEAT THE WARDEN" : "SURVIVE 5 MINUTES"),
             Gold, 1);
        text(278, 451, "XP " + std::to_string(p.xp) + "/" + std::to_string(xpNeeded(p.level)),
             Muted, 1);
        text(452, 451, "START  MENU", Muted, 1);
        if (p.hp <= 0 && w.phase == Phase::Playing) {
            box(80, 190, 480, 80, {12, 19, 29, 220});
            center(206, "YOU ARE DOWN", Red, 3);
            center(245, "AN ALLY CAN REVIVE YOU", White, 2);
        }
    }
    void panel(int x, int y, int w, int h) {
        box(x + 5, y + 5, w, h, {0, 0, 0, 110});
        box(x, y, w, h, {17, 26, 39, 248});
        frame(x, y, w, h, {65, 86, 99});
        box(x, y, 3, h, {70, 131, 125});
        for (int side : {0, 1}) {
            int xx = side ? x + w - 1 : x;
            int toward = side ? -1 : 1;
            line(xx, y, xx + toward * 13, y, Gold);
            line(xx, y + h - 1, xx + toward * 13, y + h - 1, Gold);
            box(xx - 1, y + 5, 3, 3, {163, 131, 78});
            box(xx - 1, y + h - 9, 3, 3, {163, 131, 78});
        }
    }
    void upgrades(const Player &p, int selection, bool online) {
        panel(48, 104, 544, 298);
        center(123, "LEVEL UP", Gold, 3);
        center(154, online ? "CHOOSE QUICKLY - BATTLE CONTINUES" : "CHOOSE YOUR NEXT POWER", Muted,
               1);
        auto options = offers(p);
        for (int i = 0; i < 3; i++) {
            int y = 181 + i * 61;
            box(68, y, 504, 52, i == selection ? Color{33, 63, 70} : Panel);
            frame(68, y, 504, 52, i == selection ? Teal : Color{42, 56, 72});
            static const int icons[] = {IconPower, IconHaste, IconMultishot,
                                        IconFrost, IconBoots, IconVitality};
            sprites.draw(icons[options[i]], 87, y + 43);
            text(69, y + 3, std::to_string(i + 1), Gold, 1);
            text(112, y + 8, upgradeName(options[i]), i == selection ? Teal : White, 2);
            text(112, y + 31, upgradeDesc(options[i]), Muted, 1);
        }
        center(380, "UP/DOWN SELECT   A/ENTER CONFIRM", Muted, 1);
    }
};
} // namespace nf
