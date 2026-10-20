#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace nf {
constexpr float Pi = 3.14159265f, Step = 1.f / 60.f, Arena = 1600;
constexpr int MaxPlayers = 4, MaxEnemies = 240, MaxShots = 256, MaxGems = 160;
struct Vec {
    float x = 0, y = 0;
    Vec operator+(Vec b) const { return {x + b.x, y + b.y}; }
    Vec operator-(Vec b) const { return {x - b.x, y - b.y}; }
    Vec operator*(float n) const { return {x * n, y * n}; }
};
inline float length(Vec v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}
inline Vec unit(Vec v) {
    float l = length(v);
    return l > .001f ? v * (1 / l) : Vec{};
}
inline Vec limit(Vec p) {
    return {std::clamp(p.x, 24.f, Arena - 24), std::clamp(p.y, 24.f, Arena - 24)};
}
struct Input {
    float x = 0, y = 0;
    int choice = -1;
};
struct Player {
    bool active = false;
    Vec pos{800, 800};
    float hp = 100, invuln = 0, cooldown = 0, auraTimer = 0, revive = 0;
    int level = 1, xp = 0, pending = 0, kills = 0;
    std::array<int, 6> upgrades{};
};
struct Enemy {
    Vec pos;
    float hp = 0, flash = 0;
    int type = 0;
    uint32_t id = 0;
};
struct Shot {
    Vec pos, vel;
    float life = 0, damage = 0;
    int owner = 0;
};
struct Gem {
    Vec pos;
    int value = 1;
};
struct Spark {
    Vec pos;
    float life;
    int kind;
};
enum class Phase : uint8_t { Playing, Defeat, Victory };
inline int xpNeeded(int level) {
    return 8 + level * 5;
}
inline float maxHp(const Player &p) {
    return 100.f + p.upgrades[5] * 25;
}
inline const char *upgradeName(int n) {
    static const char *names[] = {"POWER",       "HASTE",       "MULTISHOT",
                                  "FROST ORBIT", "SWIFT BOOTS", "VITALITY"};
    return names[n];
}
inline const char *upgradeDesc(int n) {
    static const char *desc[] = {"+30% BOLT DAMAGE",        "FASTER AUTO ATTACK",
                                 "ONE MORE MAGIC BOLT",     "DAMAGE NEARBY ENEMIES",
                                 "MOVE AND COLLECT FASTER", "+25 HEALTH AND HEAL"};
    return desc[n];
}
inline std::array<int, 3> offers(const Player &p) {
    std::array<int, 3> r{};
    int start = ((p.level - p.pending) * 3) % 6;
    for (int i = 0; i < 3; i++)
        r[i] = (start + i * 2) % 6;
    return r;
}
struct World {
    std::array<Player, MaxPlayers> players{};
    std::vector<Enemy> enemies;
    std::vector<Shot> shots;
    std::vector<Gem> gems;
    std::vector<Spark> sparks;
    Phase phase = Phase::Playing;
    float time = 0, spawnClock = 0;
    uint32_t rng = 0x1234abcd, nextId = 1, ticks = 0;
    bool bossSpawned = false, bossKilled = false;
    uint32_t random() {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return rng;
    }
    float randf() { return (random() % 10000) / 10000.f; }
    void addPlayer(int id) {
        players[id] = Player{};
        players[id].active = true;
        players[id].pos = {800.f + id * 28, 800};
        int reference = 0;
        for (auto &p : players)
            if (p.active)
                reference = std::max(reference, p.level);
        players[id].pending = reference - 1;
        players[id].level = reference;
    }
    void reset() {
        *this = World{};
        addPlayer(0);
    }
    void effect(Vec p, int kind) {
        if (sparks.size() < 120)
            sparks.push_back({p, .35f, kind});
    }
    void award(int xp) {
        for (auto &p : players)
            if (p.active) {
                p.xp += xp;
                while (p.xp >= xpNeeded(p.level)) {
                    p.xp -= xpNeeded(p.level);
                    p.level++;
                    p.pending++;
                }
            }
    }
    void choose(int id, int choice) {
        auto &p = players[id];
        if (!p.active || p.pending <= 0 || choice < 0 || choice > 2)
            return;
        int u = offers(p)[choice];
        p.upgrades[u]++;
        p.pending--;
        if (u == 5)
            p.hp = std::min(maxHp(p), p.hp + 50);
    }
    void spawn(int type) {
        if (enemies.size() >= MaxEnemies)
            return;
        std::vector<Vec> live;
        for (auto &p : players)
            if (p.active && p.hp > 0)
                live.push_back(p.pos);
        if (live.empty())
            return;
        Vec c = live[random() % live.size()];
        float a = randf() * Pi * 2;
        Vec pos = limit(c + Vec{std::cos(a), std::sin(a)} * 420);
        float health =
            type == 3 ? 1800.f : (type == 2 ? 85.f : (type == 1 ? 13.f : 28.f)) * (1 + time / 230);
        enemies.push_back({pos, health, 0, type, nextId++});
    }
    void step(const std::array<Input, 4> &inputs, float dt = Step) {
        if (phase != Phase::Playing)
            return;
        ++ticks;
        time += dt;
        int alive = 0, active = 0;
        for (int i = 0; i < 4; i++) {
            auto &p = players[i];
            if (!p.active)
                continue;
            active++;
            choose(i, inputs[i].choice);
            if (p.hp <= 0)
                continue;
            alive++;
            Vec move = {inputs[i].x, inputs[i].y};
            if (length(move) > 1)
                move = unit(move);
            p.pos = limit(p.pos + move * (dt * (135 + 12 * p.upgrades[4])));
            p.invuln = std::max(0.f, p.invuln - dt);
            p.cooldown -= dt;
            p.auraTimer -= dt;
            if (p.cooldown <= 0 && !enemies.empty()) {
                const Enemy *near = nullptr;
                float distance = 540;
                for (auto &e : enemies) {
                    float d = length(e.pos - p.pos);
                    if (d < distance) {
                        distance = d;
                        near = &e;
                    }
                }
                if (near) {
                    Vec dir = unit(near->pos - p.pos);
                    int count = std::min(7, 1 + p.upgrades[2]);
                    for (int k = 0; k < count && shots.size() < MaxShots; k++) {
                        float a = std::atan2(dir.y, dir.x) + (k - (count - 1) * .5f) * .16f;
                        shots.push_back({p.pos,
                                         {std::cos(a) * 330, std::sin(a) * 330},
                                         1.65f,
                                         18.f * (1 + .3f * p.upgrades[0]),
                                         i});
                    }
                    p.cooldown = std::max(.12f, .68f / (1 + .22f * p.upgrades[1]));
                }
            }
            if (p.upgrades[3] > 0 && p.auraTimer <= 0) {
                p.auraTimer = .5f;
                for (auto &e : enemies)
                    if (e.hp > 0 && length(e.pos - p.pos) < 48 + 6 * p.upgrades[3]) {
                        e.hp -= 7 * p.upgrades[3];
                        e.flash = .12f;
                        if (e.hp <= 0)
                            p.kills++;
                    }
                effect(p.pos, 2);
            }
        }
        if (!alive) {
            phase = Phase::Defeat;
            return;
        }
        // Revive only advances while a living ally remains close.
        for (auto &p : players)
            if (p.active && p.hp <= 0) {
                bool near = false;
                for (auto &q : players)
                    if (q.active && q.hp > 0 && length(q.pos - p.pos) < 55)
                        near = true;
                p.revive = near ? p.revive + dt : 0;
                if (p.revive >= 3) {
                    p.hp = maxHp(p) * .5f;
                    p.invuln = 2;
                    p.revive = 0;
                    effect(p.pos, 1);
                }
            }
        spawnClock -= dt;
        while (spawnClock <= 0) {
            spawnClock += std::max(.10f, .9f - time * .0022f) / std::sqrt(float(active));
            spawn(time > 35 && random() % 5 == 0 ? 2 : (time > 15 && random() % 3 == 0 ? 1 : 0));
        }
        if (time >= 240 && !bossSpawned) {
            if (enemies.size() >= MaxEnemies)
                enemies.pop_back();
            spawn(3);
            bossSpawned = true;
        }
        for (auto &e : enemies) {
            e.flash = std::max(0.f, e.flash - dt);
            if (e.hp <= 0)
                continue;
            Player *target = nullptr;
            float d = 1e9;
            for (auto &p : players)
                if (p.active && p.hp > 0 && length(p.pos - e.pos) < d) {
                    d = length(p.pos - e.pos);
                    target = &p;
                }
            if (!target)
                continue;
            float speed = e.type == 3 ? 43 : (e.type == 2 ? 32 : (e.type == 1 ? 88 : 52));
            Vec dir = unit(target->pos - e.pos);
            e.pos = limit(e.pos + dir * (dt * (speed + time * .04f)));
            float radius = e.type == 3 ? 25 : (e.type == 2 ? 15 : 10);
            for (auto &p : players)
                if (p.active && p.hp > 0 && p.invuln <= 0 && length(p.pos - e.pos) < radius + 9) {
                    p.hp = std::max(0.f, p.hp - (e.type == 3 ? 30.f : 12.f));
                    p.invuln = .8f;
                    e.pos = limit(e.pos - dir * 16);
                    effect(p.pos, 0);
                }
        }
        for (auto &s : shots) {
            s.pos = s.pos + s.vel * dt;
            s.life -= dt;
            if (s.life <= 0)
                continue;
            for (auto &e : enemies)
                if (e.hp > 0 &&
                    length(s.pos - e.pos) < (e.type == 3 ? 29 : (e.type == 2 ? 19 : 14))) {
                    e.hp -= s.damage;
                    e.flash = .12f;
                    s.life = 0;
                    effect(s.pos, 1);
                    if (e.hp <= 0)
                        players[s.owner].kills++;
                    break;
                }
        }
        shots.erase(std::remove_if(shots.begin(), shots.end(), [](auto &s) { return s.life <= 0; }),
                    shots.end());
        for (auto &e : enemies)
            if (e.hp <= 0) {
                if (e.type == 3) {
                    bossKilled = true;
                    award(80);
                } else if (gems.size() < MaxGems)
                    gems.push_back({e.pos, e.type == 2 ? 4 : 1});
                else {
                    auto &g = gems[random() % gems.size()];
                    g.value += e.type == 2 ? 4 : 1;
                }
                effect(e.pos, 1);
            }
        enemies.erase(
            std::remove_if(enemies.begin(), enemies.end(), [](auto &e) { return e.hp <= 0; }),
            enemies.end());
        for (auto &g : gems) {
            for (auto &p : players)
                if (p.active && p.hp > 0) {
                    float d = length(p.pos - g.pos);
                    if (d < 18) {
                        award(g.value);
                        g.value = 0;
                        break;
                    }
                    if (d < 65 + 12 * p.upgrades[4])
                        g.pos = g.pos + unit(p.pos - g.pos) * dt * 230;
                }
        }
        gems.erase(std::remove_if(gems.begin(), gems.end(), [](auto &g) { return g.value == 0; }),
                   gems.end());
        for (auto &s : sparks)
            s.life -= dt;
        sparks.erase(
            std::remove_if(sparks.begin(), sparks.end(), [](auto &s) { return s.life <= 0; }),
            sparks.end());
        if (time >= 300 && bossKilled)
            phase = Phase::Victory;
    }
};
} // namespace nf
