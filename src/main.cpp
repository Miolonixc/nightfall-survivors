#include "render.hpp"
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>
#ifndef NIGHTFALL_VERSION
#define NIGHTFALL_VERSION "0.1.0"
#endif
namespace nf {
void require(bool ok, const char *label) {
    if (!ok)
        throw std::runtime_error(label);
}
int selfTest() {
    try {
        World w;
        w.reset();
        std::array<Input, 4> in{};
        in[0].x = 1;
        w.spawnClock = 100;
        w.step(in);
        require(w.players[0].pos.x > 800, "movement");
        w.players[0].pos = {1590, 5};
        for (int i = 0; i < 120; i++)
            w.step(in);
        require(w.players[0].pos.x <= 1576 && w.players[0].pos.y >= 24, "arena bounds");
        w.award(100);
        require(w.players[0].level > 1 && w.players[0].pending > 0, "level and experience");
        int pending = w.players[0].pending;
        auto options = offers(w.players[0]);
        w.choose(0, 0);
        require(w.players[0].pending == pending - 1 && w.players[0].upgrades[options[0]] == 1,
                "upgrade choice");
        w.reset();
        w.spawnClock = 100;
        w.enemies.push_back({{850, 800}, 10, 0, 0, 1});
        for (int i = 0; i < 120 && w.players[0].kills == 0; i++)
            w.step({});
        require(w.players[0].kills == 1 && !w.gems.empty(), "auto attack and drop");
        w.players[0].pos = w.gems[0].pos;
        w.step({});
        require(w.gems.empty() && w.players[0].xp == 1, "gem pickup");
        w.reset();
        w.spawnClock = 100;
        w.addPlayer(1);
        w.players[1].hp = 0;
        for (int i = 0; i < 185; i++)
            w.step({});
        require(w.players[1].hp > 0, "ally revival");
        w.players[0].hp = 0;
        w.players[1].hp = 0;
        w.step({});
        require(w.phase == Phase::Defeat, "party defeat");
        w.reset();
        w.time = 240;
        w.spawnClock = 100;
        w.step({});
        require(w.bossSpawned && w.enemies[0].type == 3, "boss spawn");
        w.enemies[0].hp = 0;
        w.step({});
        require(w.bossKilled, "boss reward");
        w.time = 300;
        w.step({});
        require(w.phase == Phase::Victory, "victory condition");
        w.reset();
        for (int i = 0; i < MaxEnemies; i++)
            w.enemies.push_back({{100, 100}, 20, 0, i % 4, uint32_t(i)});
        for (int i = 0; i < MaxShots; i++)
            w.shots.push_back({{300, 300}, {1, 2}, 1, 20, 0});
        for (int i = 0; i < MaxGems; i++)
            w.gems.push_back({{400, 400}, 1});
        auto data = snapshot(w);
        require(data.b.size() < MaxPacket, "packet budget");
        Reader r{data.b};
        require(r.u() == 3, "snapshot type");
        auto copy = decodeState(r);
        require(copy.enemies.size() == MaxEnemies && copy.shots.size() == MaxShots &&
                    copy.gems.size() == MaxGems,
                "snapshot roundtrip");
        for (size_t n = 0; n < data.b.size(); n += 37) {
            std::vector<uint8_t> shortened(data.b.begin(), data.b.begin() + n);
            bool rejected = false;
            try {
                Reader bad{shortened};
                bad.u();
                decodeState(bad);
            } catch (...) {
                rejected = true;
            }
            require(rejected, "truncated packet rejection");
        }
        Writer invalid;
        invalid.u(0x7fc00000);
        bool rejected = false;
        try {
            Reader bad{invalid.b};
            bad.f();
        } catch (...) {
            rejected = true;
        }
        require(rejected, "NaN rejection");
        w.reset();
        w.addPlayer(1);
        w.players[0].hp = w.players[1].hp = 100000;
        for (int i = 0; i < 60 * 330; i++) {
            for (auto &p : w.players)
                if (p.pending)
                    w.choose(int(&p - &w.players[0]), 0);
            w.step({});
            require(w.enemies.size() <= MaxEnemies && w.gems.size() <= MaxGems &&
                        w.shots.size() <= MaxShots,
                    "entity budgets");
        }
        std::cout << "PASS simulation, combat, progression, revive, boss, packet validation, "
                     "330-second load test\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\n";
        return 1;
    }
}
struct Sound {
    SDL_AudioDeviceID device = 0;
    int notesSent = 0;
    Sound() {
        SDL_AudioSpec want{};
        want.freq = 44100;
        want.format = AUDIO_S16SYS;
        want.channels = 2;
        want.samples = 1024;
        device = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    }
    ~Sound() {
        if (device)
            SDL_CloseAudioDevice(device);
    }
    void play(float hz, float duration = .08f) {
        if (!device || SDL_GetQueuedAudioSize(device) > 48000)
            return;
        const size_t frames = size_t(44100 * duration);
        std::vector<int16_t> samples(frames * 2);
        for (size_t i = 0; i < frames; ++i) {
            float env = 1.f - float(i) / frames;
            auto sample = int16_t(std::sin(i * hz * 2 * Pi / 44100) * 7000 * env * env);
            samples[2 * i] = samples[2 * i + 1] = sample;
        }
        if (SDL_QueueAudio(device, samples.data(), uint32_t(samples.size() * 2)) == 0) {
            ++notesSent;
            SDL_PauseAudioDevice(device, 0);
        } else
            std::cerr << "Audio queue error: " << SDL_GetError() << std::endl;
    }
};
struct Controls {
    SDL_GameController *pad = nullptr;
    SDL_JoystickID instance = -1;
    std::string exitReason;
    bool confirm = false, back = false, pause = false, up = false, down = false, left = false,
         right = false, quit = false;
    ~Controls() {
        if (pad)
            SDL_GameControllerClose(pad);
    }
    void open(int index) {
        if (!pad && SDL_IsGameController(index)) {
            pad = SDL_GameControllerOpen(index);
            if (pad)
                instance = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad));
        }
    }
    void poll() {
        confirm = back = pause = up = down = left = right = false;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
                exitReason = "SDL_QUIT";
            }
            if (e.type == SDL_CONTROLLERDEVICEADDED)
                open(e.cdevice.which);
            if (e.type == SDL_CONTROLLERDEVICEREMOVED && e.cdevice.which == instance) {
                SDL_GameControllerClose(pad);
                pad = nullptr;
                instance = -1;
            }
            if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                switch (e.key.keysym.sym) {
                case SDLK_RETURN:
                case SDLK_SPACE:
                    confirm = true;
                    break;
                case SDLK_ESCAPE:
                    back = true;
                    break;
                case SDLK_p:
                    pause = true;
                    break;
                case SDLK_UP:
                case SDLK_w:
                    up = true;
                    break;
                case SDLK_DOWN:
                case SDLK_s:
                    down = true;
                    break;
                case SDLK_LEFT:
                case SDLK_a:
                    left = true;
                    break;
                case SDLK_RIGHT:
                case SDLK_d:
                    right = true;
                    break;
                default:
                    break;
                }
            }
            if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (e.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_A:
                    confirm = true;
                    break;
                case SDL_CONTROLLER_BUTTON_B:
                    back = true;
                    break;
                case SDL_CONTROLLER_BUTTON_START:
                    pause = true;
                    break;
                case SDL_CONTROLLER_BUTTON_BACK:
                    back = true;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    up = true;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    down = true;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    left = true;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    right = true;
                    break;
                default:
                    break;
                }
            }
        }
    }
    Input movement() {
        const auto *k = SDL_GetKeyboardState(nullptr);
        Input in;
        in.x = float(k[SDL_SCANCODE_D] || k[SDL_SCANCODE_RIGHT]) -
               float(k[SDL_SCANCODE_A] || k[SDL_SCANCODE_LEFT]);
        in.y = float(k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN]) -
               float(k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP]);
        if (pad) {
            float x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) / 32767.f,
                  y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) / 32767.f;
            if (std::abs(x) > .20f)
                in.x = x;
            if (std::abs(y) > .20f)
                in.y = y;
            in.x += SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) -
                    SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
            in.y += SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) -
                    SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP);
        }
        Vec m{in.x, in.y};
        if (length(m) > 1)
            m = unit(m);
        in.x = m.x;
        in.y = m.y;
        return in;
    }
};
Input autoplay(const World &w, int id) {
    Input in;
    if (id < 0 || id > 3)
        return in;
    auto &p = w.players[id];
    Vec direction{};
    float nearest = 180;
    for (auto &e : w.enemies) {
        float d = length(e.pos - p.pos);
        if (d < nearest) {
            nearest = d;
            direction = unit(p.pos - e.pos);
        }
    }
    if (length(direction) < .1f) {
        const Gem *target = nullptr;
        float d = 400;
        for (auto &g : w.gems)
            if (length(g.pos - p.pos) < d) {
                d = length(g.pos - p.pos);
                target = &g;
            }
        direction = target ? unit(target->pos - p.pos)
                           : Vec{std::cos(w.time * .6f), std::sin(w.time * .6f)};
    }
    if (p.pos.x < 150)
        direction.x = .8f;
    if (p.pos.x > Arena - 150)
        direction.x = -.8f;
    if (p.pos.y < 150)
        direction.y = .8f;
    if (p.pos.y > Arena - 150)
        direction.y = -.8f;
    direction = unit(direction);
    in.x = direction.x;
    in.y = direction.y;
    if (p.pending)
        in.choice = 0;
    return in;
}
} // namespace nf
int main(int argc, char **argv) {
    using namespace nf;
    int frames = -1, port = Port;
    bool headless = false, demo = false, fast = false, fullscreen = false, host = false,
         uiTest = false, audioTest = false, stress = false, benchmark = false;
    std::string join, capture, report, readyFile, auditFile;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        auto value = [&]() {
            if (i + 1 >= argc)
                throw std::runtime_error("Missing argument");
            return std::string(argv[++i]);
        };
        try {
            if (arg == "--version") {
                std::cout << NIGHTFALL_VERSION << "\n";
                return 0;
            } else if (arg == "--self-test")
                return selfTest();
            else if (arg == "--headless")
                headless = true;
            else if (arg == "--demo")
                demo = true;
            else if (arg == "--stress") {
                stress = true;
                demo = true;
                benchmark = true;
            } else if (arg == "--benchmark") {
                benchmark = true;
                demo = true;
            } else if (arg == "--audio-test")
                audioTest = true;
            else if (arg == "--ui-test")
                uiTest = true;
            else if (arg == "--fast")
                fast = true;
            else if (arg == "--fullscreen")
                fullscreen = true;
            else if (arg == "--host")
                host = true;
            else if (arg == "--join")
                join = value();
            else if (arg == "--frames")
                frames = std::stoi(value());
            else if (arg == "--port")
                port = std::stoi(value());
            else if (arg == "--capture")
                capture = value();
            else if (arg == "--audit")
                auditFile = value();
            else if (arg == "--report")
                report = value();
            else if (arg == "--ready-file")
                readyFile = value();
            else if (arg == "--help") {
                std::cout << "Nightfall Survivors " NIGHTFALL_VERSION
                             "\n--host | --join IPV4 [--port 40440] --fullscreen\n--self-test "
                             "--demo --stress --benchmark --audio-test --headless --frames N "
                             "--fast --capture BMP --report JSON\n";
                return 0;
            } else
                throw std::runtime_error("Unknown argument: " + arg);
        } catch (const std::exception &e) {
            std::cerr << e.what() << "\n";
            return 2;
        }
    }
    if (port < 1024 || port > 65535) {
        std::cerr << "Invalid port\n";
        return 2;
    }
    if (headless) {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) != 0) {
        std::cerr << SDL_GetError() << "\n";
        return 1;
    }
    const char *videoDriver = SDL_GetCurrentVideoDriver();
    if (!headless && videoDriver &&
        (std::string(videoDriver) == "offscreen" || std::string(videoDriver) == "dummy")) {
        std::cerr << "No physical display backend: " << videoDriver
                  << ". Use the firmware SDL2 library.\n";
        SDL_Quit();
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_Window *window = SDL_CreateWindow("Nightfall Survivors", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 640, 480,
                                          (headless ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN) |
                                              (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
    if (!window) {
        std::cerr << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }
    uint32_t renderFlags =
        headless ? uint32_t(SDL_RENDERER_SOFTWARE) : uint32_t(SDL_RENDERER_ACCELERATED);
    bool usesVsync = !headless && !fast;
    if (usesVsync)
        renderFlags |= SDL_RENDERER_PRESENTVSYNC;
    SDL_Renderer *r = SDL_CreateRenderer(window, -1, renderFlags);
    if (!r) {
        usesVsync = false;
        r = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!r) {
        std::cerr << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_RenderSetLogicalSize(r, 640, 480);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_ShowCursor(SDL_DISABLE);
    SDL_RendererInfo rendererInfo{};
    SDL_GetRendererInfo(r, &rendererInfo);
    std::cout << "VIDEO " << SDL_GetCurrentVideoDriver() << " RENDERER "
              << (rendererInfo.name ? rendererInfo.name : "unknown") << " AUDIO "
              << (SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "none")
              << " PACING " << (usesVsync ? "vsync" : fast ? "unlimited" : "timer") << std::endl;
    int result = 0;
    {
        Paint paint{r, {}};
        Controls controls;
        for (int i = 0; i < SDL_NumJoysticks(); i++)
            controls.open(i);
        Sound sound;
        std::cout << "CONTROLLER " << (controls.pad ? SDL_GameControllerName(controls.pad) : "none")
                  << " AUDIO_DEVICE " << (sound.device ? "open" : SDL_GetError()) << std::endl;
        bool muted = false;
        Network net;
        World world;
        world.reset();
        enum View { Title, Playing, Join, Pause, Help };
        View view = Title;
        int selection = 0, upgradeSelection = 0, ipPart = 0;
        std::array<int, 4> ip{192, 168, 1, 100};
        std::string message;
        std::string hostIP = localIP();
        std::string dataDir = ".";
        char *pref = SDL_GetPrefPath("Miolonixc", "Nightfall");
        if (pref) {
            dataDir = pref;
            SDL_free(pref);
        }
        if (const char *env = std::getenv("NIGHTFALL_DATA"))
            dataDir = env;
        std::ifstream config(dataDir + "/settings.txt");
        int m = 0;
        if (config) {
            config >> m;
            for (int &i : ip) {
                config >> i;
                i = std::clamp(i, 0, 255);
            }
            muted = m != 0;
        }
        auto ipString = [&]() {
            return std::to_string(ip[0]) + "." + std::to_string(ip[1]) + "." +
                   std::to_string(ip[2]) + "." + std::to_string(ip[3]);
        };
        auto start = [&](bool online) {
            net.stop();
            world.reset();
            message.clear();
            if (online && !net.host(port)) {
                message = net.error;
                return;
            }
            view = Playing;
            selection = upgradeSelection = 0;
        };
        if (host)
            start(true);
        else if (!join.empty()) {
            if (net.join(join, port))
                view = Playing;
            else
                message = net.error;
        } else if (demo) {
            start(false);
            world.time = 55;
            for (int i = 0; i < 45; i++)
                world.spawn(i % 3);
            world.players[0].level = 6;
            world.players[0].upgrades = {2, 2, 2, 1, 1, 1};
            world.players[0].hp = 125;
        }
        if (stress) {
            world.time = 230;
            world.spawnClock = 10000;
            world.enemies.clear();
            for (int i = 0; i < MaxEnemies; ++i) {
                float angle = i * 2 * Pi / MaxEnemies;
                float radius = 80.f + float(i % 11) * 13.f;
                world.enemies.push_back(
                    {world.players[0].pos + Vec{std::cos(angle), std::sin(angle)} * radius,
                     1000000.f, 0, i % 3, uint32_t(i + 1)});
            }
            world.players[0].upgrades = {4, 4, 6, 4, 2, 4};
            world.players[0].hp = maxHp(world.players[0]);
        }
        float accumulator = 0;
        auto previous = std::chrono::steady_clock::now();
        auto launchTime = previous;
        int frame = 0, lastKills = 0, lastLevel = 1, queuedChoice = -1, maxParty = 1;
        unsigned visited = 0;
        float lastHp = 100;
        bool readyWritten = false;
        std::vector<float> frameTimes;
        int peakEnemies = 0, peakShots = 0;
        uint64_t controllerEvents = 0;
        uint32_t lastAudit = 0;
        float travelDistance = 0;
        Vec lastPlayerPos = world.players[0].pos;
        while (!controls.quit && (frames < 0 || frame < frames)) {
            auto now = std::chrono::steady_clock::now();
            if (frame > 0 && frameTimes.size() < 36000)
                frameTimes.push_back(
                    std::chrono::duration<float, std::milli>(now - previous).count());
            float elapsed = std::chrono::duration<float>(now - previous).count();
            previous = now;
            accumulator += fast ? Step : std::min(elapsed, .20f);
            if (uiTest) {
                int button = -1;
                switch (frame) {
                case 0:
                case 22:
                case 33:
                case 41:
                case 51:
                case 54:
                    button = SDL_CONTROLLER_BUTTON_A;
                    break;
                case 10:
                case 52:
                    button = SDL_CONTROLLER_BUTTON_START;
                    break;
                case 11:
                case 20:
                case 34:
                case 43:
                    button = SDL_CONTROLLER_BUTTON_B;
                    break;
                case 21:
                case 30:
                case 31:
                case 32:
                case 53:
                    button = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
                    break;
                case 40:
                case 42:
                case 50:
                    button = SDL_CONTROLLER_BUTTON_DPAD_UP;
                    break;
                default:
                    break;
                }
                if (button >= 0) {
                    SDL_Event event{};
                    event.type = SDL_CONTROLLERBUTTONDOWN;
                    event.cbutton.button = uint8_t(button);
                    SDL_PushEvent(&event);
                }
            }
            controls.poll();
            if (benchmark) {
                controls.confirm = controls.back = controls.pause = controls.up = controls.down =
                    controls.left = controls.right = false;
            }
            if (audioTest && frame % 60 == 0 && frame < 180)
                sound.play(440.f + 220.f * (frame / 60), .25f);
            if (!muted && !benchmark && (view == Title || view == Pause || view == Join)) {
                if (controls.confirm)
                    sound.play(620, .09f);
                else if (controls.back)
                    sound.play(280, .06f);
                else if (controls.up || controls.down || controls.left || controls.right)
                    sound.play(420, .045f);
            }
            bool consumed = false;
            if (view == Title) {
                if (controls.up)
                    selection = (selection + 5) % 6;
                if (controls.down)
                    selection = (selection + 1) % 6;
                if (controls.confirm || controls.pause) {
                    if (selection == 0)
                        start(false);
                    if (selection == 1)
                        start(true);
                    if (selection == 2) {
                        view = Join;
                        ipPart = 0;
                    }
                    if (selection == 3) {
                        view = Help;
                    }
                    if (selection == 4)
                        muted = !muted;
                    if (selection == 5) {
                        controls.quit = true;
                        controls.exitReason = "MENU_QUIT";
                    }
                    consumed = true;
                }
                if (controls.back) {
                    controls.quit = true;
                    controls.exitReason = "TITLE_BACK";
                }
            } else if (view == Join) {
                if (controls.left)
                    ipPart = (ipPart + 3) % 4;
                if (controls.right)
                    ipPart = (ipPart + 1) % 4;
                if (controls.up)
                    ip[ipPart] = (ip[ipPart] + 1) % 256;
                if (controls.down)
                    ip[ipPart] = (ip[ipPart] + 255) % 256;
                if (controls.confirm) {
                    if (ipPart < 3)
                        ipPart++;
                    else if (net.join(ipString(), port)) {
                        view = Playing;
                        message.clear();
                    }
                }
                if (controls.pause) {
                    if (net.join(ipString(), port)) {
                        view = Playing;
                        message.clear();
                    }
                }
                if (controls.back)
                    view = Title;
            } else if (view == Help) {
                if (controls.back || controls.confirm || controls.pause)
                    view = Title;
            } else if (view == Pause) {
                if (controls.up || controls.down)
                    selection = 1 - selection;
                if (controls.back || controls.pause || (controls.confirm && selection == 0)) {
                    view = Playing;
                    consumed = true;
                } else if (controls.confirm && selection == 1) {
                    net.stop();
                    view = Title;
                    selection = 0;
                }
            } else if (view == Playing) {
                int id = std::max(0, net.local);
                bool pending = world.players[id].pending > 0 && world.players[id].hp > 0;
                if (world.phase != Phase::Playing) {
                    if (controls.confirm || controls.back || controls.pause) {
                        net.stop();
                        view = Title;
                        selection = 0;
                        consumed = true;
                    }
                } else if (net.mode == Network::Client && !net.welcomed && controls.back) {
                    net.stop();
                    view = Title;
                    selection = 0;
                    consumed = true;
                } else if (controls.pause || controls.back) {
                    view = Pause;
                    selection = 0;
                    consumed = true;
                } else if (pending) {
                    if (controls.up)
                        upgradeSelection = (upgradeSelection + 2) % 3;
                    if (controls.down)
                        upgradeSelection = (upgradeSelection + 1) % 3;
                }
            }
            Input input = controls.movement();
            if (view != Playing || consumed)
                input = {};
            int id = std::max(0, net.local);
            if (demo && view == Playing)
                input = autoplay(world, id);
            if (view == Playing && world.players[id].pending > 0 && !consumed && controls.confirm) {
                queuedChoice = upgradeSelection;
                if (!muted)
                    sound.play(660, .14f);
            }
            if (queuedChoice >= 0 && view == Playing)
                input.choice = queuedChoice;
            while (accumulator >= Step) {
                accumulator -= Step;
                bool connected = net.mode != Network::Offline;
                if (connected) {
                    auto mode = net.mode;
                    net.update(world, input, Step);
                    if (mode != Network::Offline && net.mode == Network::Offline) {
                        message = net.error;
                        view = Title;
                        selection = 0;
                        std::cerr << message << "\n";
                        if (headless)
                            result = 1;
                    }
                    if (net.mode == Network::Host) {
                        world.step(net.inputs);
                        net.consumeChoices();
                    }
                } else if (view == Playing) {
                    auto &p = world.players[0];
                    if (p.pending > 0 && input.choice >= 0)
                        world.choose(0, input.choice);
                    if (p.pending == 0 || p.hp <= 0) {
                        std::array<Input, 4> in{};
                        in[0] = input;
                        world.step(in);
                    }
                }
                input.choice = -1;
                queuedChoice = -1;
            }
            id = std::max(0, net.local);
            auto &p = world.players[id];
            if (!muted && view == Playing) {
                if (p.hp < lastHp)
                    sound.play(100, .12f);
                else if (p.level > lastLevel)
                    sound.play(880, .16f);
                else if (p.kills > lastKills)
                    sound.play(330, .04f);
            }
            lastHp = p.hp;
            lastLevel = p.level;
            lastKills = p.kills;
            peakEnemies = std::max(peakEnemies, int(world.enemies.size()));
            peakShots = std::max(peakShots, int(world.shots.size()));
            Input observed = controls.movement();
            if (controls.confirm || controls.back || controls.pause || controls.up ||
                controls.down || controls.left || controls.right || std::abs(observed.x) > .25f ||
                std::abs(observed.y) > .25f)
                ++controllerEvents;
            float moved = length(world.players[id].pos - lastPlayerPos);
            if (moved < 20)
                travelDistance += moved;
            lastPlayerPos = world.players[id].pos;
            visited |= 1u << unsigned(view);
            maxParty =
                std::max(maxParty, int(std::count_if(world.players.begin(), world.players.end(),
                                                     [](const Player &p) { return p.active; })));
            if (stress) {
                world.players[0].hp = maxHp(world.players[0]);
                world.players[0].invuln = 1;
                world.phase = Phase::Playing;
            }
            paint.world(world, id);
            if (benchmark) {
                paint.box(0, 54, 640, 18, Ink);
                paint.center(
                    58, stress ? "PERFORMANCE TEST - 240 ENEMIES" : "PERFORMANCE TEST - AUTOPILOT",
                    Gold, 1);
            }
            if (view == Title) {
                paint.box(0, 0, 640, 480, {9, 15, 25, 215});
                paint.center(70, "NIGHTFALL", Gold, 5);
                paint.center(126, "S U R V I V O R S", Teal, 2);
                paint.line(190, 150, 300, 150, {80, 71, 51});
                paint.line(340, 150, 450, 150, {80, 71, 51});
                paint.diamond(320, 150, 3, Gold);
                paint.center(159, "THE LAST LIGHT IN THE GRAVEYARD", Muted, 1);
                const char *items[] = {"SOLO SURVIVAL", "HOST CO-OP", "JOIN CO-OP",
                                       "HOW TO PLAY",   "SOUND",      "QUIT"};
                for (int i = 0; i < 6; i++) {
                    int y = 194 + i * 32;
                    if (selection == i) {
                        paint.box(148, y - 7, 344, 28, {35, 48, 51});
                        paint.frame(148, y - 7, 344, 28, {121, 106, 69});
                        paint.text(159, y, ">", Teal, 2);
                    }
                    std::string label = items[i];
                    if (i == 4)
                        label += muted ? "  OFF" : "  ON";
                    paint.center(y, label, selection == i ? Gold : White, 2);
                }
                paint.center(406, "A/ENTER SELECT   B/ESC BACK", Muted, 1);
                paint.text(12, 469, "V" NIGHTFALL_VERSION, Muted, 1);
                if (!message.empty())
                    paint.center(425, message, Red, 1);
            }
            if (view == Help) {
                paint.panel(36, 76, 568, 341);
                paint.center(97, "KEEP THE LIGHT ALIVE", Gold, 3);
                paint.text(62, 144,
                           "MOVE: STICK / D-PAD / WASD\n\nMAGIC ATTACKS AUTOMATICALLY.\nCOLLECT "
                           "GREEN GEMS TO LEVEL UP.\nCHOOSE ONE OF THREE UPGRADES.\n\nSURVIVE 5 "
                           "MINUTES AND BEAT THE WARDEN.\n\nCO-OP: UP TO 4 PLAYERS, SAME "
                           "VERSION.\nJOIN THE HOST IP ON THE SAME NETWORK.\nSTAND NEAR A FALLEN "
                           "ALLY TO REVIVE.\n\nSTART: MENU   B: BACK",
                           White, 1);
                paint.center(387, "A/ENTER TO RETURN", Teal, 2);
            }
            if (view == Join) {
                paint.panel(40, 100, 560, 291);
                paint.center(122, "JOIN CO-OP", Gold, 3);
                paint.center(170, "ENTER THE HOST IPV4 ADDRESS", Muted, 1);
                for (int i = 0; i < 4; i++) {
                    char part[8];
                    std::snprintf(part, sizeof part, "%03d", ip[i]);
                    paint.box(96 + i * 116, 205, 102, 50, i == ipPart ? Color{35, 70, 74} : Panel);
                    paint.text(111 + i * 116, 219, part, i == ipPart ? Teal : White, 3);
                    if (i < 3)
                        paint.text(200 + i * 116, 230, ".", Muted, 2);
                }
                paint.center(280, "UP/DOWN NUMBER   LEFT/RIGHT PART", Muted, 1);
                paint.center(312, "A NEXT / CONNECT   START CONNECT", White, 1);
                paint.center(345, "B BACK - HOST MUST BE RUNNING", Muted, 1);
                if (!net.error.empty())
                    paint.center(374, net.error, Red, 1);
            }
            if ((view == Playing || view == Pause) && net.mode == Network::Host) {
                paint.box(0, 55, 640, 17, {12, 19, 29, 210});
                paint.text(12, 60,
                           "HOST " + hostIP + ":" + std::to_string(port) +
                               "   SHARE THIS IP WITH FRIENDS",
                           Teal, 1);
            }
            if (view == Playing && net.mode == Network::Client && !net.welcomed) {
                paint.panel(90, 175, 460, 100);
                paint.center(201, "CONNECTING...", Teal, 3);
                paint.center(245, "B TO CANCEL", Muted, 1);
            }
            if (view == Playing && world.phase == Phase::Playing && p.pending > 0 && p.hp > 0 &&
                (!net.connecting))
                paint.upgrades(p, upgradeSelection, net.mode != Network::Offline);
            if (view == Pause) {
                paint.panel(80, 150, 480, 184);
                paint.center(171, net.mode == Network::Offline ? "PAUSED" : "BATTLE CONTINUES",
                             Gold, 3);
                paint.center(226, (selection == 0 ? "> " : "  ") + std::string("RESUME"),
                             selection == 0 ? Teal : White, 2);
                paint.center(270, (selection == 1 ? "> " : "  ") + std::string("LEAVE GAME"),
                             selection == 1 ? Teal : White, 2);
            }
            if (view == Playing && world.phase != Phase::Playing) {
                paint.panel(66, 142, 508, 205);
                paint.center(163,
                             world.phase == Phase::Victory ? "DAWN HAS COME" : "THE LIGHT FADES",
                             world.phase == Phase::Victory ? Teal : Red, 3);
                paint.center(
                    216, "LEVEL " + std::to_string(p.level) + "   KILLS " + std::to_string(p.kills),
                    White, 2);
                paint.center(252, "SURVIVED " + std::to_string(int(world.time)) + " SECONDS", Muted,
                             2);
                paint.center(308, "A/ENTER RETURN TO MENU", Gold, 1);
            }
            if (!capture.empty() && frames >= 0 && frame == frames - 1) {
                SDL_Surface *s =
                    SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32, SDL_PIXELFORMAT_ARGB8888);
                if (!s ||
                    SDL_RenderReadPixels(r, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels,
                                         s->pitch) != 0 ||
                    SDL_SaveBMP(s, capture.c_str()) != 0) {
                    std::cerr << SDL_GetError() << "\n";
                    result = 1;
                }
                if (s)
                    SDL_FreeSurface(s);
            }
            if (!auditFile.empty() && SDL_GetTicks() - lastAudit >= 100) {
                lastAudit = SDL_GetTicks();
                std::ofstream audit(auditFile + ".tmp");
                audit << "{\"view\":" << int(view) << ",\"selection\":" << selection
                      << ",\"x\":" << world.players[id].pos.x
                      << ",\"y\":" << world.players[id].pos.y;
                if (controls.pad) {
                    audit << ",\"axes\":[";
                    for (int a = 0; a < 4; ++a)
                        audit << (a ? "," : "")
                              << SDL_JoystickGetAxis(SDL_GameControllerGetJoystick(controls.pad),
                                                     a);
                    audit << "]";
                }
                audit << "}\n";
                audit.close();
                std::rename((auditFile + ".tmp").c_str(), auditFile.c_str());
            }
            SDL_RenderPresent(r);
            if (!readyWritten && !readyFile.empty() && frame > 30 &&
                std::chrono::duration<float>(now - launchTime).count() > 2) {
                std::ofstream stamp(readyFile);
                stamp << NIGHTFALL_VERSION << "\n";
                readyWritten = bool(stamp);
            }
            frame++;
            // Track the successfully requested mode. Some firmware renderers omit
            // PRESENTVSYNC from GetRendererInfo despite waiting for every swap.
            // Sleeping to the same deadline then misses the next display interval.
            if (!fast && !usesVsync) {
                auto target = now + std::chrono::microseconds(16667);
                std::this_thread::sleep_until(target);
            }
        }
        if (uiTest) {
            if (visited != 31 || view != Title || net.mode != Network::Offline) {
                std::cerr << "FAIL UI navigation " << visited << "\n";
                result = 1;
            } else
                std::cout << "PASS controller-driven title, solo, pause/resume, help, IP entry, "
                             "host, leave\n";
        }
        std::ofstream settings(dataDir + "/settings.txt");
        settings << (muted ? 1 : 0);
        for (int i : ip)
            settings << " " << i;
        settings << "\n";
        double seconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - launchTime).count();
        std::sort(frameTimes.begin(), frameTimes.end());
        auto percentile = [&](float q) {
            return frameTimes.empty()
                       ? 0.f
                       : frameTimes[std::min(frameTimes.size() - 1, size_t(q * frameTimes.size()))];
        };
        std::cout << "PERF frames=" << frame << " seconds=" << seconds
                  << " fps=" << (seconds > 0 ? frame / seconds : 0)
                  << " p95_ms=" << percentile(.95f) << " p99_ms=" << percentile(.99f)
                  << " peak_enemies=" << peakEnemies << " peak_shots=" << peakShots
                  << " exit=" << controls.exitReason << std::endl;
        if (!report.empty()) {
            std::ofstream out(report);
            out << "{\"seconds\":" << seconds << ",\"fps\":" << (seconds > 0 ? frame / seconds : 0)
                << ",\"p95_ms\":" << percentile(.95f) << ",\"p99_ms\":" << percentile(.99f)
                << ",\"video\":\"" << SDL_GetCurrentVideoDriver() << "\",\"renderer\":\""
                << (rendererInfo.name ? rendererInfo.name : "unknown")
                << "\",\"audio_notes\":" << sound.notesSent
                << ",\"audio_open\":" << (sound.device ? "true" : "false")
                << ",\"controller_open\":" << (controls.pad ? "true" : "false")
                << ",\"exit_reason\":\"" << controls.exitReason
                << "\",\"views_visited\":" << visited << ",\"travel_distance\":" << travelDistance
                << ",\"input_frames\":" << controllerEvents << ",\"peak_enemies\":" << peakEnemies
                << ",\"peak_shots\":" << peakShots << ",\"frames\":" << frame
                << ",\"ticks\":" << world.ticks << ",\"time\":" << world.time
                << ",\"local\":" << net.local << ",\"joins\":" << net.joins
                << ",\"states\":" << net.states << ",\"active\":[";
            for (int i = 0; i < 4; i++)
                out << (i ? "," : "") << (world.players[i].active ? "true" : "false");
            out << "],\"level\":" << world.players[std::max(0, net.local)].level
                << ",\"enemies\":" << world.enemies.size() << ",\"max_party\":" << maxParty
                << ",\"network_error\":" << (net.error.empty() ? "false" : "true") << "}\n";
        }
    }
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
