# chiaki-ng fork: lower latency, Quick Setup, screenshots

This fork is based on upstream chiaki-ng (commit c8bea26, Oct 9 2026) and adds three changes.

## 1. Lower-latency video decoding

`lib/src/ffmpegdecoder.c` used to open the decoder with FFmpeg's defaults. By default
FFmpeg uses *frame threading*, which holds back several frames before it outputs the
first one. In a game stream that delay is input lag.

The decoder now sets:

- `AV_CODEC_FLAG_LOW_DELAY`, which turns off frame threading so every packet comes straight out as a frame
- `AV_CODEC_FLAG2_FAST`, a small speedup that doesn't change the picture
- slice threading for software decoding, so it still uses several CPU cores without adding delay
- one thread when a hardware decoder (NVIDIA / D3D11 / Vulkan) is in use

The Nintendo Switch port of chiaki-ng already used these settings; the desktop app did not.
If a decoder misbehaves, set the environment variable `CHIAKI_DISABLE_LOW_DELAY_DECODE=1`
to return to the old behaviour.

## 2. Quick Setup (simpler UI)

The main screen has a new **Quick Setup** button (shortcut **F3**). It offers three one-click presets:

| Preset | At home | Away (remote) | Renderer |
|---|---|---|---|
| Competitive | 1080p60, 30 Mbps | 720p60, 10 Mbps | Fast |
| Balanced | 1080p60, auto bitrate | 720p60, auto bitrate | Default |
| Quality | 1080p60, 50 Mbps | 1080p60, 20 Mbps | High Quality |

PS5 presets use H.265. PS4 gets the matching resolution and frame rate with automatic bitrate.
Every value stays editable in the full Settings screen. Quick Setup marks whichever preset
matches your current settings.

## 3. Screenshots

- Press **F12** while streaming, or use the new **Screenshot** button in the stream menu
  (Ctrl+O or your controller shortcut).
- PNGs go to `Pictures\chiaki-ng\chiaki-ng_YYYYMMDD_HHMMSS_mmm.png`.
- The screenshot is taken from the decoded video frame at full stream resolution, so menus
  and stats overlays are not in it.
- HDR streams are tone-mapped to normal (SDR) colours so the PNG looks right in any viewer.
- The PNG is converted on a background thread, so taking a screenshot doesn't stutter the stream.

## Building for Windows

The simplest route is GitHub Actions; you don't need a local toolchain.

1. Fork https://github.com/streetpea/chiaki-ng on GitHub.
2. Apply this change set to your fork, either `git am chiaki-ng-fork.patch` on a clone
   or copying the changed files over.
3. Push. Then in your fork open **Actions → "Build chiaki-ng Windows x86_64 (VC)" → Run workflow**.
4. When the run finishes, download **chiaki-ng-win_x64-VC-Release** (portable folder with `chiaki.exe`) or **chiaki-ng-win_x64-VC-installer.zip** from the run page. Artifacts are kept for 7 days.

## 4. Experimental 120 fps + stream FPS counter

- Settings → Video (PS5): the FPS dropdowns have a third option, **120 fps (Experimental)**.
  Chiaki then asks the PS5 for 120 fps when the stream starts. Sony's Remote Play is only known
  to stream 30/60 fps, so the console may ignore the request (you still get 60) or refuse the
  connection. If it refuses, switch back to 60 fps.
- The in-game stats overlay (Settings → Audio/Wifi → Show Stream Stats During Gameplay) now shows
  **fps**: the number of video frames actually received from the console each second.
  That number tells you whether 120 fps worked.
- The log records the requested rate ("Requesting 1920x1080 @ 120 fps from console").
