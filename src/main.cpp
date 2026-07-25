#include <SDL2/SDL.h>
#include <cstdio>

#include "player/player.hpp"

// Big Buck Bunny — official Blender Foundation magnet, safe test source.
constexpr const char* kMagnetUri =
    "magnet:?xt=urn:btih:dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c&dn=Big+Buck+Bunny"
    "&tr=udp://explodie.org:6969&tr=udp://tracker.coppersurfer.tk:6969"
    "&tr=udp://tracker.empire-js.us:1337&tr=udp://tracker.leechers-paradise.org:6969"
    "&tr=udp://tracker.opentrackr.org:1337&tr=wss://tracker.btorrent.xyz"
    "&tr=wss://tracker.fastcast.nz&tr=wss://tracker.openwebtorrent.com";

constexpr const char* kDownloadDir = "./downloads";

int main() {
    std::fprintf(stderr, "[MAIN] SDL_Init\n");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "[MAIN] SDL_Init FAILED: %s\n", SDL_GetError());
        return 1;
    }

    std::fprintf(stderr, "[MAIN] create player\n");
    Player player;

    std::fprintf(stderr, "[MAIN] open_torrent (this will download metadata + start streaming)\n");
    if (!player.open_torrent(kMagnetUri, kDownloadDir)) {
        std::fprintf(stderr, "[MAIN] open_torrent FAILED\n");
        SDL_Quit();
        return 1;
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
