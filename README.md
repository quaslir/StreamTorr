# StreamTorr

**Watch video while it's still downloading.** A local streaming player written in C++: pulls a file over BitTorrent in the right order and plays back whatever's already downloaded — no waiting for the full download to finish.

## Idea

A regular torrent client downloads pieces of a file in arbitrary order and requires a complete download before you can watch anything. StreamTorr downloads **sequentially** (start to end, with a priority buffer ahead of the current playback position) and plays back finished pieces as they become available — something between a torrent client and a streaming service.

## Architecture

```
[libtorrent] --sequential download--> [file on disk, filled in as it goes]
                                                |
                                    [FFmpeg demux + decode]
                                    (reads from the file via a
                                     custom blocking AVIOContext)
                                                |
                                    [SDL2 window]
                                    (video texture render +
                                     audio playback + A/V sync)
```

Core idea: don't reinvent decoding — FFmpeg handles demuxing/decoding, SDL2 handles rendering the frame to a window and playing audio. Your own code's job is mainly to (a) make sure the needed byte range is on disk before FFmpeg asks for it, and (b) keep audio and video in sync.

## Stack

- **libtorrent (rasterbar)** — torrent protocol, sequential piece priority, DHT/tracker handling
- **FFmpeg (libavformat/libavcodec)** — demuxing and decoding, with a custom `AVIOContext` that blocks/waits on pieces not yet downloaded
- **SDL2** — window, texture rendering (`SDL_Texture`/`SDL_Renderer`), audio output (`SDL_AudioDeviceID`), A/V sync driven off the audio clock (classic ffplay-style approach: audio is the master clock, video frames wait/drop to match)

## How it's meant to work

1. User provides a magnet link or `.torrent` file.
2. StreamTorr starts sequential downloading.
3. Once the initial buffer is ready, an SDL2 window opens and playback starts.
4. Download keeps running ahead of the playback position; on seek, piece priorities are re-targeted to the new range.

## Roadmap

- [ ] **Stage 1** — local file player: FFmpeg decode + SDL2 render/audio, no torrent involved yet
- [ ] **Stage 2** — libtorrent integration: download a full file in the background
- [ ] **Stage 3** — sequential download + priority buffer window ahead of playback
- [ ] **Stage 4** — custom blocking `AVIOContext` reading from a partially-downloaded file
- [ ] **Stage 5** — adaptive buffering (start playback based on actual measured download speed)
- [ ] **Stage 6** — seek handling (re-prioritizing pieces on the fly)
- [ ] **Stage 7** (optional) — alternative terminal renderer (ANSI/Sixel/Kitty) as an experimental mode

## A note on content

The technology itself (sequential-torrent streaming) is neutral and legal — the same approach works fine on, say, official Archive.org torrents or open videos from the Blender Foundation (Sintel, Big Buck Bunny), which are great for testing the pipeline without any legality concerns. What specific files a user chooses to download is outside the tool's control or responsibility.

## Status

🚧 Architecture/design stage, no code written yet.
