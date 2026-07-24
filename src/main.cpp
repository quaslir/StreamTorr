

#include <SDL2/SDL.h>
#include <cstdint>
#include <iostream>
#include <thread>
#include "player/player.hpp"
extern "C" {
#include <libavutil/samplefmt.h>
}


int main()
{
    std::cout << "[MAIN] SDL_Init\n";

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
    {
        std::cout << SDL_GetError() << '\n';
        return 1;
    }

    std::cout << "[MAIN] create player\n";

    Player player;

    std::cout << "[MAIN] open\n";
    bool opened = player.open("../assets/test_media/bbb_sunflower_1080p_60fps_normal.mp4");
    std::cout << "[MAIN] open result: " << opened << "\n";
    if (!opened) { return 1; }
    std::cout << "[MAIN] play\n";
    player.play();
    std::cout << "[MAIN] entering loop\n";
    int loop_count = 0;
    while (true) {
        player.update();
        if (++loop_count % 1000 == 0) {
            std::cout << "[MAIN] loop iteration " << loop_count << "\n";
        }
        SDL_Delay(1);
    }

    SDL_Quit();
}
