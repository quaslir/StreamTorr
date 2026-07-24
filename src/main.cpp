

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

    if(!player.open("../assets/test_media/sintel_trailer-1080p.mp4"))
    {
        std::cout << "[MAIN] open failed\n";
        return 1;
    }

    std::cout << "[MAIN] play\n";

    player.play();


    while (true)
    {
        player.update();
    }

    SDL_Quit();
}
