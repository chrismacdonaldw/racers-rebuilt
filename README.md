<h1 align="center">racers-rebuilt</h1>

<p align="center">A native Windows port of LEGO Racers (1999).</p>

<p align="center">
  <a href="https://github.com/chrismacdonaldw/racers-rebuilt/releases/latest"><img alt="Release" src="https://img.shields.io/github/v/release/chrismacdonaldw/racers-rebuilt?style=flat"></a>
  <a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/chrismacdonaldw/racers-rebuilt?style=flat"></a>
</p>

<p align="center">
  <a href="https://github.com/chrismacdonaldw/racers-rebuilt/releases/latest">Download</a> ·
  <a href="#install">Install</a> ·
  <a href="#troubleshooting">Troubleshooting</a> ·
  <a href="CONTRIBUTING.md">Contributing</a>
</p>

racers-rebuilt is a reimplementation of LEGO Racers for 64-bit Windows. It reads the game's data from your own copy, and runs at any framerate without changing the simulation.

> [!IMPORTANT]
> **No game files are included.** racers-rebuilt needs an installed copy of the original PC game.

> [!NOTE]
> v0.4.0 plays single races, circuits and their mirrored versions, time trials against the record ghost, and two-player split-screen versus, all from the main menu, with options, controls and language screens. The car builder, garage and cutscenes aren't in yet.

## Features

- One `.exe` with no installer and no extra DLLs
- Game files copied from your install once, so later launches don't need it
- Any framerate from 30 to 360 FPS, uncapped, or VSync, on a fixed simulation tick
- Automatic detection of your LEGO Racers install, with a one-time folder picker as the fallback
- Replacement music in OGG, FLAC, MP3 or WAV, with optional loop points
- Data mods that replace any file in `LEGO.JAM`
- An in-game settings overlay on <kbd>F1</kbd>

## Requirements

| Component | Minimum |
|---|---|
| OS | Windows 10 or 11, 64-bit |
| GPU | Direct3D 12 with Shader Model 6.0 |
| Game | An installed copy of LEGO Racers for PC (the folder with `LEGO.JAM`) |

Tested with the English PC release from 1999 and the 2001 re-release. [`game.version`](SETTINGS.md#game) picks which one to play.

## Install

1. Download `racers-rebuilt-windows-x64.zip` from the [latest release](https://github.com/chrismacdonaldw/racers-rebuilt/releases/latest).
2. Extract it to any folder.
3. Run `racers-rebuilt.exe`.
4. If the game isn't found, a folder picker opens. Choose the LEGO Racers folder that contains `LEGO.JAM`.

The choice is saved, so the picker only shows once.

### Game files

The first launch copies `LEGO.JAM` and the `.tun` music and `.avi` videos from your install to `game/` in the [data folder](#data-folder), about 130 MB. The 2001 release copies to `game-2001/` instead, so switching `game.version` never copies again. Later launches read that copy, so the original install can be moved or removed.

- A racers-rebuilt update that needs different files copies them again on its next launch. It needs the install to do that.
- Changing `game.path` or `game.path_2001` copies from the new folder on the next launch.
- `--game-path` reads that folder in place and copies nothing.
- If the copy fails, for example on a full disk, the game reads the install in place and the log says why.
- Deleting `game/` or `game-2001/` makes the next launch find that install and copy it again.

### How the game is found

This runs on the first launch, with `--game-path`, and when the game files need copying again. It looks for the release `game.version` names. Each location is checked in order, and the first folder with a valid `LEGO.JAM` of that release wins. A folder of the other release is skipped.

1. `--game-path <folder>` on the command line
2. `game.path` in `settings.toml`, or `game.path_2001` for the 2001 release
3. The other release's path setting
4. The folder `racers-rebuilt.exe` is in
5. The LEGO Racers entries in the Windows registry, left by the original installer
6. `%ProgramFiles(x86)%\LEGO Media\Games\LEGO Racers`, then the same under `%ProgramFiles%`, then `C:\Games\LEGO Racers`
7. The folder picker

A folder is valid when `LEGO.JAM` exists and contains the `GAMEDATA` and `MENUDATA` folders. If the picked folder fails that check, a message says why and the picker opens again. Closing the picker exits with a message.

If only the other release turns up, from steps 3 to 7 or `--game-path`, racers-rebuilt switches `game.version` to it and saves the change. It tells the releases apart by the console menu names the 2001 release added to `MENUDATA/MENUNAME.SRF`.

`--game-path` doesn't fall back. If that folder is invalid, or holds the other release while `--set game.version` asks for this one, racers-rebuilt shows the error and exits.

### Data folder

Settings, logs, the copied game files, music and mods live in the data folder, `%APPDATA%\racers-rebuilt\` by default.

To keep everything next to the exe instead, create an empty file named `portable.txt` beside `racers-rebuilt.exe`. That folder becomes the data folder.

```text
<data>/
  settings.toml
  logs/racers-rebuilt.log
  game/
  music/
  mods/
  overrides/
  presets/
```

Create `music/`, `mods/` and `overrides/` when you first need them. `presets/` appears when you save a [quick config](#quick-configs).

## Configuration

Press <kbd>F1</kbd> to open the overlay. Its menus are **Audio**, **Bindings**, **Enhancements**, **Game**, **Mods**, **Race**, **Video**, **Progress** and **Developer**. **Enhancements** holds the **Camera**, **Cheats**, **Fixes**, **Gameplay** and **Modifiers** submenus. **Developer** shows frame time, ticks per frame and startup timings, and its **Scenes** list opens test scenes and any race track in your copy.

Every setting is saved in `<data>/settings.toml`. [SETTINGS.md](SETTINGS.md) lists each one with its default and values, along with the control bindings and command line options.

```powershell
.\racers-rebuilt.exe --set video.vsync=false --set video.fps_cap=60
```

### Quick configs

A fresh install plays like the original game: every enhancement starts off. **Enhancements → Presets** switches between quick configs:

- **Default** turns every enhancement off.
- **Enhanced** turns on the port's recommended set: physics-driven computer racers, respawn, champion cars, smooth textures, anisotropic filtering, far cars and scenery, widescreen with widescreen cutscenes and a 21:9 HUD, and clipping prevention.
- **Save current as...** saves your enhancement and mod settings under a name, as a [file you can share](SETTINGS.md#quick-config-files). **Delete** removes one.

Default and Enhanced leave your mods as they are. Your own configs also restore which mods are on. Resolution, frame rate, audio volume and controls are never part of a config.

### Controls

| Key | Action |
|---|---|
| <kbd>F1</kbd> | Show or hide the overlay |
| <kbd>F9</kbd> | Cycle VSync, 30, 60 and 144 FPS, and uncapped |
| <kbd>F11</kbd> | Switch between a window and fullscreen |
| <kbd>Print Screen</kbd> | Save a screenshot to the [data folder](#data-folder) as `LEGO<n>.bmp`, with the first free number |

Driving and menu bindings are in [SETTINGS.md](SETTINGS.md#controls), and can be changed from the options screen.

## Music and mods

- **Music:** a file named after a track, such as `<data>/music/circuit1.ogg`, plays instead of the original. OGG, FLAC, MP3 and WAV work, with optional loop points.
- **Mods:** a folder in `<data>/mods/` with a `mod.toml`. Its files replace any file in `LEGO.JAM`, and it can add tracks, racers, driver parts, bricks, HQ textures and glTF models.
- **Texture packs:** `--dump-textures` writes every game image as a PNG mod to edit or upscale.
- **Overrides:** files in `<data>/overrides/` replace single game files without a mod.

Turn mods on and off in the overlay's **Mods** menu. [MODDING.md](MODDING.md) covers the folder layouts, `mod.toml`, and each kind of replacement.

## Troubleshooting

The log is at `<data>/logs/racers-rebuilt.log`. It's replaced on each launch, so copy it before starting again.

<details>
<summary>The folder picker keeps coming back</summary>

The chosen folder doesn't have a usable `LEGO.JAM`. The message before the picker reopens says why. Choose the installed game folder, not the CD or the `Save` folder.

</details>

<details>
<summary>racers-rebuilt uses the wrong game folder</summary>

Delete the `game.path` line from `<data>/settings.toml` and start again. Or start with `--game-path <folder>`.

</details>

<details>
<summary>An error appears before the window opens</summary>

Update the graphics driver. racers-rebuilt needs Direct3D 12. The log names the failing step.

</details>

<details>
<summary>The game stutters at high framerates</summary>

Turn off external framerate limiters, such as the one in the NVIDIA Control Panel. Set the framerate with <kbd>F9</kbd> or in the overlay's **Video** menu instead.

</details>

<details>
<summary>Replacement music doesn't play</summary>

Check that the file name matches the track id and the extension is supported. If the file failed to decode, the log names it and the next source plays instead: an enabled mod's track, then the original.

</details>

<details>
<summary>A mod doesn't load</summary>

Check that the folder name matches `id` in `mod.toml`, and that the mod is on under **Mods** in the overlay. Restart after changing it. The log says why a mod was skipped.

</details>

<details>
<summary>Reset all settings</summary>

Delete `<data>/settings.toml`. The defaults return on the next start, and the game folder is detected again.

</details>

## Building from source

See [CONTRIBUTING.md](CONTRIBUTING.md#building). Builds need Visual Studio 2022, CMake, Ninja and vcpkg.

## Acknowledgements

- [SDL3](https://www.libsdl.org/) for the window, input, file dialogs and GPU API
- [miniaudio](https://miniaud.io/) for audio output, mixing and decoding
- [stb_vorbis and stb_image](https://github.com/nothings/stb) for OGG Vorbis and PNG decoding
- [FFmpeg](https://ffmpeg.org/) for the intro videos' Indeo 5 decoder, built under the [LGPL-2.1-or-later](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html) with only that decoder enabled. The release uses the source of FFmpeg's [`n9.0.2`](https://github.com/FFmpeg/FFmpeg/tree/n9.0.2) tag, built by [`vcpkg-ports/ffmpeg-indeo5`](vcpkg-ports/ffmpeg-indeo5)
- [Dear ImGui](https://github.com/ocornut/imgui) for the overlay
- [toml++](https://github.com/marzer/tomlplusplus) for settings and mod manifests
- [cgltf](https://github.com/jkuhlmann/cgltf) for reading glTF models from mods
- [glm](https://github.com/g-truc/glm) for rendering math
- The [LDraw Parts Library](https://library.ldraw.org/) for car piece models, driver minifigs and torso and head prints, under CC BY 4.0 and CC BY 2.0, shipped as converted data in `content/ldraw` ([notice](content/ldraw/NOTICE.md))
- [doctest](https://github.com/doctest/doctest) for unit tests
- The [LEGO Racers decompilation](https://github.com/isledecomp/racers) by the isledecomp team, used as a reference for game behavior and file formats

## License

[GPL-3.0](LICENSE). The release zip's `licenses` folder holds the license of each library built into the exe.

racers-rebuilt isn't affiliated with or endorsed by the LEGO Group. LEGO is a trademark of the LEGO Group.
