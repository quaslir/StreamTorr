#include <SDL2/SDL.h>
#include <cstdio>

#include "player/player.hpp"

// Big Buck Bunny — official Blender Foundation magnet, safe test source.
constexpr const char* kDownloadDir = "./downloads";

int main(int argc, char * argv[]) {
    if(argc < 2) return -1;
    std::string path_or_url{argv[1]};
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        return 1;
    }

    Player player;

    std::fprintf(stderr, "[MAIN] open_torrent (this will download metadata + start streaming)\n");
    if (!player.open_torrent(path_or_url, kDownloadDir)) {
        std::fprintf(stderr, "[MAIN] open_torrent FAILED\n");
        if(!player.open(path_or_url)) {
        SDL_Quit();
        return 1;
        }
    }

    std::fprintf(stderr, "[MAIN] play\n");
    player.play();

    std::fprintf(stderr, "[MAIN] entering render loop\n");
    long loop_count = 0;
    while (true) {
        player.update();

        if (player.state() == PlayerState::Finished || player.state() == PlayerState::Stopped) {
            std::fprintf(stderr, "[MAIN] playback ended (state=%d)\n", static_cast<int>(player.state()));
            break;
        }

        if (++loop_count % 5000 == 0) {
            std::fprintf(stderr, "[MAIN] loop iteration %ld\n", loop_count);
        }

        SDL_Delay(1);
    }

    SDL_Quit();
    return 0;
}
