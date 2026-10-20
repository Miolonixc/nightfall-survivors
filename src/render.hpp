#pragma once
#include "network.hpp"
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
    void color(Color c) { SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a); }
    void box(int x, int y, int w, int h, Color c) {
        color(c);
        SDL_Rect a{x, y, w, h};
        SDL_RenderFillRect(r, &a);
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
        static const std::map<char, std::array<uint8_t, 7>> font = {
            {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
            {'C', {14, 17, 16, 16, 16, 17, 14}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
            {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
            {'G', {14, 17, 16, 23, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
            {'I', {31, 4, 4, 4, 4, 4, 31}},      {'J', {7, 2, 2, 2, 18, 18, 12}},
            {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
            {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 25, 21, 19, 19, 17}},
            {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
            {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
            {'S', {15, 16, 16, 14, 1, 1, 30}},   {'T', {31, 4, 4, 4, 4, 4, 4}},
            {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
            {'W', {17, 17, 17, 21, 21, 21, 10}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
            {'Y', {17, 17, 10, 4, 4, 4, 4}},     {'Z', {31, 1, 2, 4, 8, 16, 31}},
            {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
            {'2', {14, 17, 1, 2, 4, 8, 31}},     {'3', {30, 1, 1, 14, 1, 1, 30}},
            {'4', {2, 6, 10, 18, 31, 2, 2}},     {'5', {31, 16, 16, 30, 1, 1, 30}},
            {'6', {14, 16, 16, 30, 17, 17, 14}}, {'7', {31, 1, 2, 4, 8, 8, 8}},
            {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 1, 14}},
            {':', {0, 4, 4, 0, 4, 4, 0}},        {'.', {0, 0, 0, 0, 0, 12, 12}},
            {'-', {0, 0, 0, 31, 0, 0, 0}},       {'/', {1, 2, 2, 4, 8, 8, 16}},
            {'+', {0, 4, 4, 31, 4, 4, 0}},       {'!', {4, 4, 4, 4, 4, 0, 4}},
            {'>', {16, 8, 4, 2, 4, 8, 16}},      {'?', {14, 17, 1, 2, 4, 0, 4}},
            {'(', {2, 4, 8, 8, 8, 4, 2}},        {')', {8, 4, 2, 2, 2, 4, 8}}};
        int start = x;
        for (char ch : s) {
            if (ch == '\n') {
                y += 9 * scale;
                x = start;
                continue;
            }
            auto it = font.find(ch);
            if (it != font.end())
                for (int row = 0; row < 7; row++)
                    for (int col = 0; col < 5; col++)
                        if (it->second[row] & (1 << (4 - col)))
                            box(x + col * scale, y + row * scale, scale, scale, c);
            x += 6 * scale;
        }
    }
    void center(int y, const std::string &s, Color c = White, int scale = 2) {
        text((640 - int(s.size()) * 6 * scale) / 2, y, s, c, scale);
    }
    Vec screen(Vec p) { return p - camera + Vec{320, 240}; }
    void person(Vec pos, int id, float time, bool down = false, bool hurt = false) {
        Vec s = screen(pos);
        int x = int(s.x), y = int(s.y);
        circle(x, y + 11, 12, {6, 12, 20, 120});
        if (down) {
            box(x - 12, y + 3, 24, 8, Muted);
            box(x - 4, y - 5, 8, 24, White);
            return;
        }
        if (hurt && int(time * 20) % 2)
            return;
        Color c = playerColor(id);
        int bob = int(std::sin(time * 8 + id) * 2);
        box(x - 10, y - 9 + bob, 20, 22, c);
        box(x - 13, y + 3 + bob, 6, 12, c);
        box(x + 7, y + 3 + bob, 6, 12, c);
        box(x - 6, y - 18 + bob, 12, 12, {239, 201, 164});
        box(x - 10, y - 21 + bob, 20, 7, {45, 58, 80});
        box(x - 5, y - 29 + bob, 10, 12, c);
        box(x - 12, y - 17 + bob, 24, 4, c);
        box(x - 3, y - 13 + bob, 3, 2, Ink);
        box(x + 3, y - 13 + bob, 3, 2, Ink);
        box(x - 7, y + 13 + bob, 5, 5, {48, 60, 77});
        box(x + 3, y + 13 + bob, 5, 5, {48, 60, 77});
    }
    void monster(const Enemy &e, float t) {
        Vec s = screen(e.pos);
        int x = int(s.x), y = int(s.y);
        if (x < -40 || x > 680 || y < -40 || y > 520)
            return;
        Color c = e.flash > 0
                      ? White
                      : (e.type == 3 ? Color{194, 92, 118}
                                     : (e.type == 2 ? Color{135, 112, 169} : Color{109, 158, 130}));
        int bob = int(std::sin(t * 5 + e.id) * 2);
        if (e.type == 1) {
            box(x - 9, y - 6 + bob, 18, 15, {151, 111, 184});
            for (int i = 0; i < 3; i++) {
                box(x - 23 + i * 5, y - 11 + i * 3 + bob, 7, 8, c);
                box(x + 16 - i * 5, y - 11 + i * 3 + bob, 7, 8, c);
            }
        } else {
            int k = e.type == 3 ? 2 : 1;
            circle(x, y + 12 * k, 11 * k, {5, 11, 19, 120});
            box(x - 10 * k, y - 6 * k + bob, 20 * k, 21 * k, c);
            box(x - 8 * k, y - 17 * k + bob, 16 * k, 14 * k, c);
            box(x - 14 * k, y + 2 * k + bob, 5 * k, 9 * k, c);
            box(x + 9 * k, y + 2 * k + bob, 5 * k, 9 * k, c);
            if (e.type == 3) {
                box(x - 18, y - 39 + bob, 36, 9, Gold);
                box(x - 18, y - 45 + bob, 6, 9, Gold);
                box(x - 3, y - 45 + bob, 6, 9, Gold);
                box(x + 12, y - 45 + bob, 6, 9, Gold);
            }
        }
        box(x - 6, y - 11 + bob, 4, 3, Gold);
        box(x + 3, y - 11 + bob, 4, 3, Gold);
        box(x - 4, y + 1 + bob, 8, 3, Ink);
    }
    void world(const World &w, int local) {
        int id = std::clamp(local, 0, 3);
        camera = w.players[id].pos;
        camera.x = std::clamp(camera.x, 320.f, Arena - 320);
        camera.y = std::clamp(camera.y, 240.f, Arena - 240);
        box(0, 0, 640, 480, {17, 29, 36});
        for (int yy = int(camera.y - 260) / 40; yy < (camera.y + 260) / 40; yy++)
            for (int xx = int(camera.x - 340) / 40; xx < (camera.x + 340) / 40; xx++) {
                uint32_t h = (uint32_t(xx) * 73856093u) ^ (uint32_t(yy) * 19349663u);
                Vec s = screen({xx * 40.f, yy * 40.f});
                int x = int(s.x), y = int(s.y);
                box(x, y, 39, 39, h % 3 ? Color{22, 36, 42} : Color{25, 40, 43});
                if (h % 9 == 0) {
                    box(x + 7, y + 17, 2, 5, {42, 64, 59});
                    box(x + 12, y + 14, 2, 6, {42, 64, 59});
                }
                if (h % 31 == 0) {
                    box(x + 21, y + 13, 11, 15, {47, 60, 67});
                    box(x + 23, y + 9, 7, 7, {47, 60, 67});
                    box(x + 24, y + 14, 5, 2, {25, 36, 46});
                    box(x + 26, y + 12, 1, 7, {25, 36, 46});
                }
                if (h % 53 == 0) {
                    circle(x + 17, y + 22, 10, {65, 56, 37, 100});
                    box(x + 15, y + 17, 4, 10, {203, 170, 95});
                    box(x + 16, y + 13 + int(w.time * 5) % 2, 2, 4, Gold);
                }
            }
        for (auto &g : w.gems) {
            Vec s = screen(g.pos);
            diamond(int(s.x), int(s.y), 4, g.value > 1 ? Gold : Teal);
            box(int(s.x), int(s.y) - 2, 1, 2, White);
        }
        for (auto &e : w.enemies)
            monster(e, w.time);
        for (int i = 0; i < 4; i++) {
            auto &p = w.players[i];
            if (!p.active)
                continue;
            if (p.upgrades[3] && p.hp > 0) {
                for (int k = 0; k < 6; k++) {
                    float a = w.time * 2 + k * Pi / 3;
                    Vec s =
                        screen(p.pos + Vec{std::cos(a), std::sin(a)} * (48 + 6 * p.upgrades[3]));
                    diamond(int(s.x), int(s.y), 3, {117, 190, 255});
                }
            }
            person(p.pos, i, w.time, p.hp <= 0, p.invuln > 0);
            Vec s = screen(p.pos);
            text(int(s.x) - 10, int(s.y) - 41, "P" + std::to_string(i + 1), playerColor(i), 1);
            if (p.hp <= 0) {
                box(int(s.x) - 20, int(s.y) + 24, 40, 4, Panel);
                box(int(s.x) - 20, int(s.y) + 24, int(40 * p.revive / 3), 4, Teal);
            }
        }
        for (auto &s : w.shots) {
            Vec p = screen(s.pos);
            Vec trail = p - unit(s.vel) * 10;
            line(int(trail.x), int(trail.y), int(p.x), int(p.y), playerColor(s.owner));
            diamond(int(p.x), int(p.y), 4, White);
        }
        for (auto &s : w.sparks) {
            Vec p = screen(s.pos);
            int rad = int((.35f - s.life) * 45);
            for (int k = 0; k < 6; k++) {
                float a = k * Pi / 3;
                box(int(p.x + std::cos(a) * rad), int(p.y + std::sin(a) * rad), 3, 3,
                    s.kind == 0 ? Red : Gold);
            }
        }
        box(0, 0, 640, 53, Ink);
        box(0, 53, 640, 1, {49, 65, 77});
        text(16, 12, "NIGHTFALL", Teal, 2);
        auto &p = w.players[id];
        box(157, 12, 141, 12, {51, 40, 51});
        box(157, 12, int(141 * std::clamp(p.hp / maxHp(p), 0.f, 1.f)), 12, Red);
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
        box(x, y, 4, h, Teal);
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
            text(80, y + 9, std::to_string(i + 1), Gold, 3);
            text(112, y + 8, upgradeName(options[i]), i == selection ? Teal : White, 2);
            text(112, y + 31, upgradeDesc(options[i]), Muted, 1);
        }
        center(380, "UP/DOWN SELECT   A/ENTER CONFIRM", Muted, 1);
    }
};
} // namespace nf
