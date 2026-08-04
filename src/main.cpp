#include <SDL2/SDL.h>
#include <SDL_ttf.h>
#include <cstdio>

#include "player/player.hpp"
#include "render/loading_screen.hpp"

constexpr const char *kDownloadDir = "./downloads";

int main(int argc, char *argv[]) {
    if (argc < 2)
        return -1;
    std::string path_or_url{argv[1]};
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        return 1;
    }
    if (TTF_Init() != 0) {
        SDL_Quit();
        return 1;
    }
    LoadingScreen loading;
    if(!loading.open()) {
            SDL_Quit();
            return 1;
    }


    auto player = std::make_shared<Player>();
    player->set_progress_callback([&loading](TorrentProgress p) {
        const char *stage_name = p.stage == TorrentStage::FetchingMetadata      ? "Fetching metadata"
                                 : p.stage == TorrentStage::DownloadingHeadTail ? "Buffering"
                                 : p.stage == TorrentStage::OpeningStream       ? "Opening"
                                                                                : "Ready";
        loading.update(stage_name, p.percent);
    });

    if (!player->open_torrent(path_or_url, kDownloadDir)) {
        if (!player->open_local(path_or_url)) {
            SDL_Quit();
            return 1;
        }
    }

    loading.close();

    player->play();
    while (true) {
        player->update();

        if (player->state() == PlayerState::Finished || player->state() == PlayerState::Stopped) {
            break;
        }

        SDL_Delay(1);
    }

player.reset();

TTF_Quit();
SDL_Quit();
    return 0;
}
