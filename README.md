# StreamTorr

**A native C++ torrent-streaming media player.** Play video directly from a magnet link while it downloads — no waiting for the file to finish, no separate torrent client. Built on FFmpeg, libtorrent, and SDL2.

> Solo project. No CI badges here because I'm not going to lie to you about having a pipeline you can't see. It builds, it plays, it seeks, it survives bad networks. Read on.

---

## Table of Contents

- [What this is](#what-this-is)
- [Features](#features)
- [Architecture](#architecture)
- [How the streaming actually works](#how-the-streaming-actually-works)
- [A/V sync](#av-sync)
- [Threading model](#threading-model)
- [Controls](#controls)
- [Building](#building)
- [Project layout](#project-layout)
- [Testing](#testing)
- [Legal test content](#legal-test-content)
- [Known limitations / roadmap](#known-limitations--roadmap)
- [Hard-won lessons](#hard-won-lessons)
- [License](#license)

---

## What this is

StreamTorr is a desktop video player that treats a torrent swarm as a seekable, streamable data source instead of "download first, play later." You give it a magnet link, it fetches metadata, prioritizes the parts of the file it needs *right now* (the head for the container, the tail for the moov/Cues index, and a moving window around the current playback position), and starts decoding and rendering frames while the rest of the file fills in around you.

It also plays local files directly, no torrent required.

## Features

- **Torrent streaming** — play while downloading, with adaptive piece prioritization that follows the playback head and re-asserts itself every second regardless of network hiccups
- **Seeking on an incomplete file** — seeking on a torrent proactively re-prioritizes the target byte range instead of just hoping the data shows up
- **Adaptive buffering** — a `Buffering` state with hysteresis (low/high watermarks) and a hard timeout safety net, so a slow network degrades gracefully into a pause-and-recover instead of a freeze
- **Local file playback** — same pipeline, no torrent client involved
- **A/V sync** that doesn't drift — audio-driven clock with buffer-compensated PTS, plus a wall-clock fallback so video never stalls waiting on audio that hasn't arrived yet
- **Play/pause, seek ±10s** (keyboard and click)
- **Real volume control** — actually attenuates the audio via `SDL_MixAudioFormat`, not just a UI decoration
- **Fullscreen** toggle (key or click), tracks real window state instead of a hand-rolled flag
- **Subtitles** — both text (SRT/ASS/`mov_text`, rendered via SDL_ttf) and bitmap (PGS, palette-decoded to RGBA and blitted as a texture)
- **Staged loading screen** — an honest, monotonic progress bar across metadata → head/tail buffering → stream opening → ready, in its own lightweight window
- **Live download-progress bar** in the in-video control panel, separate from the playback-position bar
- **HiDPI/Retina-aware** rendering (real renderer output size, not logical window size)
- Runs on macOS and Linux

## Architecture

```
TorrentClient + TorrentIOContext  →  Demuxer  →  VideoDecoder / AudioDecoder / SubtitleDecoder
                                                        │
                                          VideoResampler / AudioResampler
                                                        │
                                    Pipeline (owns everything, background demux_thread_)
                                                        │
                                    FrameQueue<smart_frame>  (video / audio, thread-safe, bounded)
                                                        │
                                    Player (Clock, A/V sync, SDL render + audio, pause, seek, state machine)
                                                        │
                                    UIOverlay (progress bar, time, volume, fullscreen, subtitles)
```

Each layer only knows about the one directly below it. `Pipeline` is the only thing that touches the demuxer/decoders directly; `Player` never does.

### Core classes

| Class | Responsibility |
|---|---|
| `TorrentClient` | libtorrent session wrapper — adds sources, tracks metadata, prioritizes byte ranges, waits on piece availability, reports download progress |
| `TorrentIOContext` | Custom `AVIOContext` bridging libtorrent's on-disk (sparse) file to FFmpeg's blocking read/seek callbacks |
| `Demuxer` | Thin wrapper over `AVFormatContext` — opens files or custom I/O, finds streams, reads packets, classifies them by type |
| `VideoDecoder` / `AudioDecoder` / `SubtitleDecoder` | Wrap `AVCodecContext`; subtitle decoding uses the legacy `avcodec_decode_subtitle2` path since subtitles never migrated to `send_packet`/`receive_frame` |
| `VideoResampler` / `AudioResampler` | Convert whatever the decoder hands back (arbitrary pixel format, sample format, sometimes 10-bit) into one fixed target format, opened lazily on the first real frame |
| `Pipeline` | Orchestrates everything above from a single background thread; owns the mutex that protects the demuxer and decoders |
| `FrameQueue<T>` | Bounded, thread-safe queue with blocking `push`/`pop`, `close()`, and `clear()` — the seams where producer and consumer threads meet |
| `Clock` | An `std::atomic<double>` wrapped in a class, because that's all a playback clock needs to be |
| `Player` | The state machine — `Ready` / `Playing` / `Paused` / `Buffering` / `Stopped` / `Finished` — and the only thing that touches SDL rendering and the audio device |
| `UIOverlay` | Draws the control panel, handles clicks against cached hit-boxes, renders subtitles (text and bitmap) |
| `LoadingScreen` | A separate, minimal SDL window shown only during the torrent-opening sequence, driven entirely from the main thread |

## How the streaming actually works

1. `add_source()` on the magnet, then wait for `has_metadata()`.
2. Prioritize a head window (container header, first frames) and a tail window (index atoms / Cues — MKV and non-faststart MP4 often need the end of the file just to open).
3. Wait for the head window to reach a usable threshold, then hand a custom `AVIOContext` to FFmpeg.
4. From here on, `TorrentIOContext::read_packet()` is the only thing that talks to disk. On every read it re-prioritizes the swarm around the current byte offset and blocks (with a timeout) until libtorrent confirms the requested range is actually on disk.
5. A background thread (`TorrentClient::alert_loop`) independently re-asserts priority on the *currently active* window once a second, so a stall in `read_packet` (blocked waiting on the network) doesn't also stall the swarm's sense of urgency.

Seeking re-estimates a target byte offset from `seconds / duration * file_size`, prioritizes it immediately (before touching the demuxer), and only then performs the actual `av_seek_frame`. Byte estimation is linear and therefore imprecise on VBR content, but the prioritized window is generously sized to absorb the error.

## A/V sync

- The audio thread updates `Clock` using a **buffer-compensated** PTS: `frame_end_pts − (bytes_still_queued_in_SDL / bytes_per_second)`. Using the raw PTS at the moment of queuing causes a constant offset equal to however much audio is sitting in the device buffer waiting to play.
- The main thread compares the next video frame's PTS against `max(clock.get_time(), estimated_clock)`, where `estimated_clock` is a wall-clock extrapolation from the moment the clock was last primed. This fallback exists because video is not guaranteed to be first in decode order — without it, a file where audio takes a moment to arrive can deadlock video waiting on a clock that never advances.
- `clock_primed` is reset on every seek so the wall-clock fallback re-anchors to the new position instead of extrapolating from stale data.

## Threading model

Four threads, four jobs:

| Thread | Owns | Never does |
|---|---|---|
| **Main** | Event polling, UI, render present, state transitions | Anything that blocks on the network |
| **Demux** (`Pipeline::demux_loop`) | Reading packets, decoding video/audio/subtitles, pushing to queues | Anything touching SDL |
| **Audio** (`Player::audio_loop`) | Draining the audio queue, updating `Clock`, feeding SDL's audio device | Decoding — it only ever reads already-decoded frames |
| **Alert** (`TorrentClient::alert_loop`) | Polling libtorrent alerts, re-asserting swarm priority | **Anything touching SDL or UI callbacks** — this one bit us once; see below |

The single biggest recurring class of bug in this project was one thread holding a lock (or touching a resource) across a call that could block indefinitely, stalling something that had nothing to do with the original operation. The fixes that stuck:

- `FrameQueue::push()` never happens while `pipeline_mutex_` is held — decode, collect frames locally, release the lock, *then* push.
- `Pipeline::seek()` clears queues *before* acquiring the mutex (to unstick anything blocked mid-`push`), then again *after* flushing decoders (to purge anything that snuck in during the window).
- Every SDL-touching callback is guaranteed to run on the main thread. A progress callback that used to fire from `alert_loop` was removed for exactly this reason — SDL's rendering is not thread-safe on any platform this targets.

## Controls

| Input | Action |
|---|---|
| `Space` / click on video | Play / pause |
| `←` / `→` | Seek ±10 seconds |
| `F` | Enter fullscreen |
| `Esc` | Exit fullscreen |
| Click progress bar | Seek to position |
| Click volume slider | Set volume |
| Click play/pause icon | Play / pause |
| Click fullscreen icon | Toggle fullscreen |

## Building

### Requirements

- CMake ≥ 3.20
- A C++20 compiler (Clang or GCC; MSVC path exists but is far less exercised)
- Network access on first configure — libtorrent, SDL2, SDL_ttf, and fmt are fetched via `FetchContent`; FFmpeg is found via `pkg-config` if present, otherwise built statically from source

### macOS / Linux

```bash
git clone <this-repo>
cd StreamTorr
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

Binary lands at `build/src/stream-torr`.

```bash
# local file
./src/stream-torr /path/to/video.mp4

# magnet link
./src/stream-torr 'magnet:?xt=urn:btih:...'
```

### Debug builds

`CMAKE_BUILD_TYPE=Debug` enables `-g3`, frame pointers, and **ThreadSanitizer** (given how much of this project is concurrent, that's not optional tooling — it's load-bearing). ASan/UBSan and TSan are mutually exclusive at the compiler level; pick one build config per debugging session.

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

### Formatting

```bash
cmake --build . --target format
```

Applies the repo's `.clang-format` (LLVM base style) to every `.cpp`/`.hpp`.

## Project layout

```
src/
  torrent/        TorrentClient — libtorrent session, prioritization, progress
  io/             TorrentIOContext — custom AVIOContext bridging torrent storage to FFmpeg
  decoder/        Demuxer, VideoDecoder, AudioDecoder, SubtitleDecoder, Clock
  media/          VideoResampler, AudioResampler
  pipeline/       Pipeline — orchestration, background demux thread
  player/         Player — state machine, A/V sync, SDL ownership
  render/         VideoRenderer, AudioRenderer, UIOverlay, LoadingScreen
include/          Public headers, mirroring src/
tests/            Catch2 unit + integration tests
assets/           Test media, fonts
```

## Testing

Catch2, run via CTest:

```bash
cd build
ctest --output-on-failure
```

Integration tests exercise the real pipeline end-to-end against a bundled test file (decoding real packets, draining real queues from real threads) rather than mocking the FFmpeg boundary — the bugs that mattered in this project lived at the boundaries between threads and libraries, and mocks would have hidden every one of them.

## Legal test content

Streaming-tests use Blender Foundation open-content films, which are explicitly distributed for redistribution and testing:

- **Big Buck Bunny** — `magnet:?xt=urn:btih:dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c&dn=Big+Buck+Bunny`
- **Sintel** — `magnet:?xt=urn:btih:08ada5a7a6183aae1e09d831df6748d566095a10&dn=Sintel`

Direct HTTP mirrors also work for local-file testing (e.g. `sintel_trailer-1080p.mp4`, `bbb_sunflower_1080p_60fps_normal.mp4`).

## Known limitations / roadmap

- **No rolling window** — the whole file accumulates on disk; there's no eviction of already-played pieces. Fine for anything that fits on disk; would need a custom libtorrent storage backend to do properly.
- **DHT state isn't persisted between runs** — every cold start re-bootstraps DHT from scratch (typically a few seconds to ~30s before peers show up).
- **Byte-offset seeking is linear**, not index-aware — accurate enough in practice given the generous re-prioritization window, but a VBR file with a very uneven bitrate curve could occasionally seek a few seconds off from the requested timestamp on the *first* attempt before the demuxer corrects to the nearest keyframe.
- **PGS (bitmap) subtitles** decode and render, but there's no styling/positioning polish beyond "blit the palette-converted bitmap at its reported coordinates, scaled to the current window."
- **Multi-track subtitle/audio selection** is not exposed in the UI yet — the demuxer picks streams via FFmpeg's own heuristic (`av_find_best_stream`) rather than offering a picker.

## Hard-won lessons

A short, honest list of things that broke in ways worth remembering, because most of them were subtle enough to reappear if the underlying invariant is ever violated again:

- **Copy-paste between `open()` and `open_with_io_context()` is a recurring failure mode.** Both must independently call `avformat_find_stream_info` and search for every stream type (video, audio, *and* subtitle) — one has, more than once, quietly lost one of these during a refactor.
- **Never call `flush()`/`decode()` on a decoder that was never `init()`'d.** Files without audio, or with an unsupported subtitle codec, will otherwise dereference a null `AVCodecContext`.
- **`SDL_MixAudioFormat`'s volume argument is `0–SDL_MIX_MAXVOLUME` (128), not `0.0–1.0`.** Forgetting the multiply, or applying it after truncating to `int`, silently produces near-silence instead of an error.
- **10-bit sources (`yuv420p10le`) need `SWS_ACCURATE_RND`** in the scaler, or bright/saturated regions dither incorrectly during the bit-depth conversion.
- **A paused SDL audio device never drains its queue.** Any loop that waits for the queued-audio size to drop below a threshold needs a timeout, or pausing the device deadlocks that thread — and if that thread is also the only consumer of a bounded queue whose producer lives on another thread, the whole pipeline stalls behind it.
- **`FrameQueue::close()` must wake blocked writers, not just blocked readers.** A `push()` blocked on a full queue needs its own condition variable notified on close, or `stop()` can hang forever joining a thread that's still waiting to push.

## License

*MIT*
