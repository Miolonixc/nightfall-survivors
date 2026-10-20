#pragma once
#include "game.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace nf {
constexpr uint32_t Magic = 0x4e463031, Protocol = 1;
constexpr int Port = 40440;
constexpr size_t MaxPacket = 32768;
struct Writer {
    std::vector<uint8_t> b;
    void u(uint32_t n) {
        for (int i = 0; i < 4; i++)
            b.push_back(uint8_t(n >> (i * 8)));
    }
    void f(float n) {
        uint32_t v;
        std::memcpy(&v, &n, 4);
        u(v);
    }
    void v(Vec p) {
        f(p.x);
        f(p.y);
    }
};
struct Reader {
    const std::vector<uint8_t> &b;
    size_t at = 0;
    uint32_t u() {
        if (at + 4 > b.size())
            throw std::runtime_error("Short packet");
        uint32_t n = 0;
        for (int i = 0; i < 4; i++)
            n |= uint32_t(b[at++]) << (i * 8);
        return n;
    }
    float f() {
        uint32_t v = u();
        float n;
        std::memcpy(&n, &v, 4);
        if (!std::isfinite(n) || std::abs(n) > 1e8)
            throw std::runtime_error("Invalid number");
        return n;
    }
    Vec v() {
        float x = f(), y = f();
        return {x, y};
    }
    int count(int cap) {
        uint32_t n = u();
        if (n > uint32_t(cap))
            throw std::runtime_error("Invalid count");
        return int(n);
    }
    void end() {
        if (at != b.size())
            throw std::runtime_error("Trailing data");
    }
};
inline Writer snapshot(const World &w) {
    Writer a;
    a.u(3);
    a.u(w.ticks);
    a.f(w.time);
    a.u(uint32_t(w.phase));
    a.u(w.bossSpawned);
    a.u(w.bossKilled);
    for (auto &p : w.players) {
        a.u(p.active);
        a.v(p.pos);
        a.f(p.hp);
        a.f(p.invuln);
        a.f(p.revive);
        a.u(p.level);
        a.u(p.xp);
        a.u(p.pending);
        a.u(p.kills);
        for (int u : p.upgrades)
            a.u(u);
    }
    a.u(w.enemies.size());
    for (auto &e : w.enemies) {
        a.v(e.pos);
        a.f(e.hp);
        a.f(e.flash);
        a.u(e.type);
        a.u(e.id);
    }
    a.u(w.shots.size());
    for (auto &s : w.shots) {
        a.v(s.pos);
        a.v(s.vel);
        a.f(s.life);
        a.f(s.damage);
        a.u(s.owner);
    }
    a.u(w.gems.size());
    for (auto &g : w.gems) {
        a.v(g.pos);
        a.u(g.value);
    }
    return a;
}
inline World decodeState(Reader &r) {
    World w;
    w.ticks = r.u();
    w.time = r.f();
    w.phase = Phase(r.count(2));
    w.bossSpawned = r.count(1);
    w.bossKilled = r.count(1);
    for (auto &p : w.players) {
        p.active = r.count(1);
        p.pos = r.v();
        p.hp = r.f();
        p.invuln = r.f();
        p.revive = r.f();
        p.level = r.count(10000);
        p.xp = r.count(1000000);
        p.pending = r.count(10000);
        p.kills = r.count(10000000);
        for (int &u : p.upgrades)
            u = r.count(10000);
    }
    int n = r.count(MaxEnemies);
    while (n--) {
        Enemy e;
        e.pos = r.v();
        e.hp = r.f();
        e.flash = r.f();
        e.type = r.count(3);
        e.id = r.u();
        w.enemies.push_back(e);
    }
    n = r.count(MaxShots);
    while (n--) {
        Shot s;
        s.pos = r.v();
        s.vel = r.v();
        s.life = r.f();
        s.damage = r.f();
        s.owner = r.count(3);
        w.shots.push_back(s);
    }
    n = r.count(MaxGems);
    while (n--) {
        Gem g;
        g.pos = r.v();
        g.value = r.count(1000000);
        w.gems.push_back(g);
    }
    r.end();
    return w;
}
inline void nonblock(int fd) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
#ifdef SO_NOSIGPIPE
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof yes);
#endif
}
struct Channel {
    int fd = -1;
    std::vector<uint8_t> rx, tx;
    size_t sent = 0;
    float idle = 0;
    void close() {
        if (fd >= 0)
            ::close(fd);
        fd = -1;
        rx.clear();
        tx.clear();
        sent = 0;
    }
    bool queue(const Writer &a) {
        if (a.b.size() > MaxPacket || tx.size() + a.b.size() > 131072)
            return false;
        Writer size;
        size.u(a.b.size());
        tx.insert(tx.end(), size.b.begin(), size.b.end());
        tx.insert(tx.end(), a.b.begin(), a.b.end());
        return true;
    }
    bool pump(std::vector<std::vector<uint8_t>> &packets) {
        if (fd < 0)
            return false;
        uint8_t buf[8192];
        for (int loops = 0; loops < 16; loops++) {
            ssize_t n = recv(fd, buf, sizeof buf, 0);
            if (n > 0) {
                rx.insert(rx.end(), buf, buf + n);
                idle = 0;
                if (rx.size() > 131072)
                    return false;
            } else if (n == 0)
                return false;
            else {
                if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                    return false;
                break;
            }
        }
        size_t at = 0;
        while (rx.size() - at >= 4) {
            uint32_t n = 0;
            for (int i = 0; i < 4; i++)
                n |= uint32_t(rx[at + i]) << (i * 8);
            if (n < 4 || n > MaxPacket)
                return false;
            if (rx.size() - at < n + 4)
                break;
            packets.emplace_back(rx.begin() + at + 4, rx.begin() + at + 4 + n);
            at += 4 + n;
            if (packets.size() > 128)
                return false;
        }
        if (at)
            rx.erase(rx.begin(), rx.begin() + at);
        while (sent < tx.size()) {
#ifdef MSG_NOSIGNAL
            int flags = MSG_NOSIGNAL;
#else
            int flags = 0;
#endif
            ssize_t n = send(fd, tx.data() + sent, tx.size() - sent, flags);
            if (n > 0)
                sent += n;
            else {
                if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
                    break;
                return false;
            }
        }
        if (sent == tx.size()) {
            tx.clear();
            sent = 0;
        }
        return true;
    }
};
inline std::string localIP() {
    ifaddrs *list = nullptr;
    std::string ip = "127.0.0.1";
    if (getifaddrs(&list) == 0) {
        for (auto *p = list; p; p = p->ifa_next)
            if (p->ifa_addr && p->ifa_addr->sa_family == AF_INET) {
                auto *a = reinterpret_cast<sockaddr_in *>(p->ifa_addr);
                if (ntohl(a->sin_addr.s_addr) >> 24 != 127) {
                    ip = inet_ntoa(a->sin_addr);
                    break;
                }
            }
        freeifaddrs(list);
    }
    return ip;
}
struct Network {
    enum Mode { Offline, Host, Client };
    Mode mode = Offline;
    int listener = -1, local = 0;
    bool connecting = false, welcomed = false;
    float connectionTime = 0, snapshotClock = 0;
    std::string error;
    std::array<Channel, 4> channels;
    std::array<bool, 4> ready{};
    std::array<Input, 4> inputs{};
    unsigned joins = 0, states = 0;
    ~Network() { stop(); }
    void stop() {
        for (auto &c : channels)
            c.close();
        if (listener >= 0)
            ::close(listener);
        listener = -1;
        mode = Offline;
        local = 0;
        connecting = welcomed = false;
        ready.fill(false);
        inputs = {};
    }
    bool host(int port = Port) {
        stop();
        error.clear();
        listener = socket(AF_INET, SOCK_STREAM, 0);
        if (listener < 0) {
            error = "CANNOT OPEN NETWORK";
            return false;
        }
        int yes = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_ANY);
        a.sin_port = htons(port);
        if (bind(listener, reinterpret_cast<sockaddr *>(&a), sizeof a) < 0 ||
            listen(listener, 4) < 0) {
            error = "PORT IN USE OR UNAVAILABLE";
            stop();
            return false;
        }
        nonblock(listener);
        mode = Host;
        local = 0;
        return true;
    }
    bool join(const std::string &ip, int port = Port) {
        stop();
        error.clear();
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(port);
        if (inet_pton(AF_INET, ip.c_str(), &a.sin_addr) != 1) {
            error = "ENTER A VALID IPV4 ADDRESS";
            return false;
        }
        auto &c = channels[0];
        c.fd = socket(AF_INET, SOCK_STREAM, 0);
        if (c.fd < 0) {
            error = "CANNOT OPEN NETWORK";
            return false;
        }
        nonblock(c.fd);
        int rc = connect(c.fd, reinterpret_cast<sockaddr *>(&a), sizeof a);
        if (rc < 0 && errno != EINPROGRESS) {
            error = "CANNOT CONNECT TO HOST";
            stop();
            return false;
        }
        mode = Client;
        local = -1;
        connecting = true;
        connectionTime = 0;
        Writer hello;
        hello.u(0);
        hello.u(Magic);
        hello.u(Protocol);
        c.queue(hello);
        return true;
    }
    void fail(const std::string &s) {
        error = s;
        stop();
    }
    void update(World &w, Input input, float dt) {
        if (mode == Offline)
            return;
        if (mode == Host) {
            for (int attempts = 0; attempts < 4; attempts++) {
                int fd = accept(listener, nullptr, nullptr);
                if (fd < 0)
                    break;
                int slot = -1;
                for (int i = 1; i < 4; i++)
                    if (channels[i].fd < 0) {
                        slot = i;
                        break;
                    }
                if (slot < 0) {
                    ::close(fd);
                    continue;
                }
                nonblock(fd);
                channels[slot].fd = fd;
                channels[slot].idle = 0;
                ready[slot] = false;
            }
            inputs[0] = input;
            for (int i = 1; i < 4; i++) {
                auto &c = channels[i];
                if (c.fd < 0)
                    continue;
                c.idle += dt;
                std::vector<std::vector<uint8_t>> packets;
                bool ok = c.pump(packets) && c.idle < 10;
                try {
                    for (auto &packet : packets) {
                        Reader r{packet};
                        uint32_t type = r.u();
                        if (!ready[i]) {
                            if (type != 0 || r.u() != Magic || r.u() != Protocol)
                                throw std::runtime_error("Wrong version");
                            r.end();
                            ready[i] = true;
                            w.addPlayer(i);
                            joins++;
                            Writer reply;
                            reply.u(1);
                            reply.u(i);
                            c.queue(reply);
                        } else {
                            if (type != 2)
                                throw std::runtime_error("Wrong input");
                            float x = r.f(), y = r.f();
                            int choice = int(r.u()) - 1;
                            r.end();
                            if (std::abs(x) > 1 || std::abs(y) > 1 || choice < -1 || choice > 2)
                                throw std::runtime_error("Bad input");
                            inputs[i].x = x;
                            inputs[i].y = y;
                            if (choice >= 0)
                                inputs[i].choice = choice;
                        }
                    }
                } catch (const std::exception &) {
                    ok = false;
                }
                if (!ok) {
                    c.close();
                    ready[i] = false;
                    w.players[i].active = false;
                    inputs[i] = {};
                }
            }
            snapshotClock -= dt;
            if (snapshotClock <= 0) {
                snapshotClock = .05f;
                Writer s = snapshot(w);
                for (int i = 1; i < 4; i++)
                    if (ready[i] && !channels[i].queue(s)) {
                        channels[i].close();
                        ready[i] = false;
                        w.players[i].active = false;
                        inputs[i] = {};
                    }
            }
        } else {
            auto &c = channels[0];
            connectionTime += dt;
            c.idle += dt;
            if (connecting) {
                fd_set writeSet;
                FD_ZERO(&writeSet);
                FD_SET(c.fd, &writeSet);
                timeval timeout{};
                int rc = select(c.fd + 1, nullptr, &writeSet, nullptr, &timeout);
                if (rc > 0) {
                    int err = 0;
                    socklen_t len = sizeof err;
                    getsockopt(c.fd, SOL_SOCKET, SO_ERROR, &err, &len);
                    if (err) {
                        fail("HOST UNAVAILABLE");
                        return;
                    }
                    connecting = false;
                } else if (connectionTime > 6) {
                    fail("CONNECTION TIMED OUT");
                    return;
                } else
                    return;
            }
            Writer a;
            a.u(2);
            a.f(input.x);
            a.f(input.y);
            a.u(input.choice + 1);
            if (!channels[0].queue(a)) {
                fail("NETWORK TOO SLOW");
                return;
            }
            std::vector<std::vector<uint8_t>> packets;
            if (!c.pump(packets) || c.idle > 10) {
                fail("HOST DISCONNECTED");
                return;
            }
            try {
                for (auto &packet : packets) {
                    Reader r{packet};
                    auto type = r.u();
                    if (type == 1 && !welcomed) {
                        local = r.count(3);
                        if (local == 0)
                            throw std::runtime_error("Bad player");
                        r.end();
                        welcomed = true;
                    } else if (type == 3 && welcomed) {
                        w = decodeState(r);
                        states++;
                    } else
                        throw std::runtime_error("Wrong packet");
                }
            } catch (const std::exception &) {
                fail("INCOMPATIBLE NETWORK DATA");
            }
        }
    }
    void consumeChoices() {
        for (auto &i : inputs)
            i.choice = -1;
    }
};
} // namespace nf
