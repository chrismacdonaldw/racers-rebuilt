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

Tested with the English PC release.

## Install

1. Download `racers-rebuilt-windows-x64.zip` from the [latest release](https://github.com/chrismacdonaldw/racers-rebuilt/releases/latest).
2. Extract it to any folder.
3. Run `racers-rebuilt.exe`.
4. If the game isn't found, a folder picker opens. Choose the LEGO Racers folder that contains `LEGO.JAM`.

The choice is saved, so the picker only shows once.

### Game files

The first launch copies `LEGO.JAM` and the `.tun` music and `.avi` videos from your install to `game/` in the [data folder](#data-folder), about 130 MB. Later launches read that copy, so the original install can be moved or removed.

- A racers-rebuilt update that needs different files copies them again on its next launch. It needs the install to do that.
- Changing `game.path` copies from the new folder on the next launch.
- `--game-path` reads that folder in place and copies nothing.
- If the copy fails, for example on a full disk, the game reads the install in place and the log says why.
- Deleting `game/` makes the next launch find the install and copy it again.

### How the game is found

This runs on the first launch, with `--game-path`, and when the game files need copying again. Each location is checked in order. The first folder with a valid `LEGO.JAM` wins.

1. `--game-path <folder>` on the command line
2. `game.path` in `settings.toml`
3. The folder `racers-rebuilt.exe` is in
4. The LEGO Racers entries in the Windows registry, left by the original installer
5. `%ProgramFiles(x86)%\LEGO Media\Games\LEGO Racers`, then the same under `%ProgramFiles%`, then `C:\Games\LEGO Racers`
6. The folder picker

A folder is valid when `LEGO.JAM` exists and contains the `GAMEDATA` and `MENUDATA` folders. If the picked folder fails that check, a message says why and the picker opens again. Closing the picker exits with a message.

`--game-path` doesn't fall back. If that folder is invalid, racers-rebuilt shows the error and exits.

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
```

Create `music/`, `mods/` and `overrides/` when you first need them.

## Configuration

Press <kbd>F1</kbd> to open the overlay. Its menus are **Audio**, **Enhancements**, **Game** and **Video**, plus **Mods** when a mod is installed, and **Developer**. Video, audio and enhancement changes apply on the next frame. Game and mod changes apply on the next start. Every change saves when an edit finishes.

**Developer** shows frame time, ticks per frame and startup timings. Its **Scenes** list opens the test scene, a sprite test that draws 1000 UI sprites, or any race track in your copy.

### Controls

| Key | Action |
|---|---|
| <kbd>F1</kbd> | Show or hide the overlay |
| <kbd>F9</kbd> | Cycle VSync, 30, 60 and 144 FPS, and uncapped |
| <kbd>Print Screen</kbd> | Save a screenshot to the [data folder](#data-folder) as `LEGO<n>.bmp`, with the first free number |

With **Enhancements → Free camera** on:

| Key | Action |
|---|---|
| <kbd>W</kbd> <kbd>A</kbd> <kbd>S</kbd> <kbd>D</kbd> | Move |
| <kbd>Q</kbd> / <kbd>E</kbd> | Down / up |
| Arrow keys | Look |
| <kbd>Shift</kbd> | Move faster |

### Settings

Every setting lives in `<data>/settings.toml` and shows in the overlay.

| Key | Default | Values |
|---|---|---|
| `game.path` | Empty | The LEGO Racers folder |
| `game.language` | From the original installer, else `ENGLISH` | `ENGLISH`, `GERMAN`, `FRENCH`, `SPANISH`, `ITALIAN`, `DUTCH`, `DANISH`, `NORWEGI`, `SWEDISH` |
| `startup.intro_videos` | `true` | `true`, `false`. Plays the original's three intro videos at launch. Any key skips the second and third. |
| `video.vsync` | `true` | `true`, `false` |
| `video.fps_cap` | `144` | `30` to `360`, or `0` for uncapped. Applies only with VSync off. |
| `video.resolution_scale` | `1.0` | `0.25` to `2.0`. Renders the scene at this fraction of the window size. |
| `video.stretch_ui` | `false` | `true`, `false` |
| `video.widescreen` | `false` | `true`, `false`. Races and cutscenes fill the window and see more to the sides, instead of the original's 4:3 frame. |
| `audio.music_volume` | `1.0` | `0.0` to `1.0` |
| `audio.effects_volume` | `1.0` | `0.0` to `1.0` |
| `audio.voice_volume` | `1.0` | `0.0` to `1.0` |
| `enhancements.free_camera` | `false` | `true`, `false` |
| `enhancements.free_camera_speed` | `40.0` | `1.0` to `400.0` |
| `race.respawn` | `true` | `true`, `false`. Puts a racer back on the track after it falls off or stays in the air too long. Applies from the next race. |
| `race.respawn_seconds` | `3` | `1` to `30`. Seconds in the air before a respawn. Applies from the next race. |
| `race.opponents` | `5` | `0` to `5`. Computer racers in a single race, from the options screen. Applies from the next race. |
| `race.versus_laps` | `3` | `1` to `5`. Laps in a versus race, from the options screen. |
| `mods.<id>.enabled` | The mod's `enabled` value | `true`, `false` |
| `mods.overrides` | `true` | `true`, `false`. Loads `<data>/overrides/`. Applies on the next start. |
| `input.keys.<action>` | See below | The first keyboard set (arrows). Comma-separated SDL key names, such as `Up,W` or `Left Shift` |
| `input.keys2.<action>` | See below | The second keyboard set (WASD), in the same form |
| `input.keys3.<action>` | See below | The third keyboard set (keypad), in the same form |
| `input.buttons.<action>` | See below | The first gamepad's set. Comma-separated SDL gamepad names: buttons such as `a` or `dpleft`, or `lefttrigger` and `righttrigger` |
| `input.pad2.buttons.<action>` | The first gamepad's defaults | The second gamepad's set, in the same form |
| `input.p1.device` | `pad1` | `pad1`, `pad2`, `keys1`, `keys2`, `keys3`. The set player 1 drives with |
| `input.p2.device` | `pad2` | The set player 2 drives with, in the same form |
| `input.p1.any_device` | `false` | `true` lets every keyboard set and gamepad drive player 1 when racing alone |

Each `<action>` has a default in every set. The left stick always steers. The menus take every keyboard and gamepad.

| Action | `keys` | `keys2` | `keys3` | `buttons` |
|---|---|---|---|---|
| `accelerate` | `Up` | `W` | `Keypad 8` | `a,righttrigger` |
| `brake` | `Down` | `X` | `Keypad 2` | `b,lefttrigger` |
| `steer_left` | `Left` | `A` | `Keypad 4` | `dpleft` |
| `steer_right` | `Right` | `D` | `Keypad 6` | `dpright` |
| `drift` | `Space` | `Left Shift` | `Keypad Enter` | `rightshoulder,leftshoulder` |
| `use_powerup` | `Return` | `Left Alt` | `Right Shift` | `x` |
| `pause` | `Escape` | | | `start` |
| `toggle_display` | `M` | `E` | `Keypad 7` | `back` |
| `change_camera` | `Right Ctrl` | `Q` | `Keypad 3` | `y` |
| `look_back` | `Right Alt` | `Z` | `Keypad 9` | `rightstick` |
| `menu_up` | `Up` | | | `dpup` |
| `menu_down` | `Down` | | | `dpdown` |
| `menu_left` | `Left` | | | `dpleft` |
| `menu_right` | `Right` | | | `dpright` |
| `menu_confirm` | `Return,Space` | | | `a` |
| `menu_back` | `Escape,Backspace` | | | `b` |

The sets follow the original's. Each player drives with one set, as the original does, so with a gamepad attached player 1 drives with the gamepad alone. A set naming a gamepad that isn't attached falls back to the first attached gamepad not in use, then to the arrows, or WASD when the other player has the arrows. Gamepads are numbered in the order they connected. A key in both players' sets drives player 1, and the pause key of a set nobody drives still pauses. PLAYER 1 CONTROLS and PLAYER 2 CONTROLS on the options screen pick each player's set from the attached gamepads and the three keyboard sets, and rebind it. A gamepad's steering rows stay closed, as it steers with its stick and d-pad. Picking the other player's set moves them to another, as the original does.

```toml
[video]
vsync = false
fps_cap = 240

[audio]
music_volume = 0.6
```

The UI keeps a centered 4:3 area unless `video.stretch_ui` is on, which stretches it to fill the window. Races and cutscenes keep to the same area unless `video.widescreen` is on.

Out-of-range values are clamped. Unknown keys are kept when the file is saved.

### Command line

| Option | Effect |
|---|---|
| `--game-path <folder>` | Use this LEGO Racers folder and skip detection |
| `--novideo` | Skip the intro videos this run, as in the original |
| `--set <key>=<value>` | Override a setting for this run only. Repeatable. |
| `--track <folder>` | Open a race track instead of the main menu, by its `GAMEDATA` folder, e.g. `RACEC0R0` |
| `--race <folder>` | Start a race on a track instead of the main menu, by its `GAMEDATA` folder. Can't be combined with `--track`. |
| `--players 2` | With `--race`, race two players against each other in split screen, with no computer racers |
| `--cheats <plates>` | With `--race`, turn on the original's license plate cheats, comma separated, e.g. `MXPMX,PGLLRD`. Plates are entered in order, so later ones can switch earlier ones off. |
| `--cutscene <folder>/<file>` | Play a menu cutscene on a loop instead of the main menu, by its `MENUDATA` folder and file, e.g. `CIRCUIT1/CIR1-01` |

```powershell
.\racers-rebuilt.exe --set video.vsync=false --set video.fps_cap=60
```

## Music replacement

Put a file named after a track's id in `<data>/music/`. It plays instead of the original.

- The id is the name of the original `.tun` file in the game folder, in lowercase: `circuit1`, `theme`, `credits`.
- Supported formats: `.ogg`, `.flac`, `.mp3`, `.wav`.
- A track loops from start to end unless it has a loop file.

To loop part of a track, add `<id>.toml` beside it. Positions are in samples.

```toml
loop_start = 441000
loop_end = 5292000
```

Either key can be left out. Without `loop_start` the loop starts at the beginning. Without `loop_end` it runs to the end.

A file that fails to decode is logged and skipped. The next source plays instead: an enabled mod's track, then the original.

## Mods

A mod is a folder in `<data>/mods/`. The folder name is the mod's id.

```text
<data>/mods/
  hd-bricks/
    mod.toml
    files/
      GAMEDATA/COMMON/RRJMW.GDB
    music/
      circuit1.ogg
```

- `files/` mirrors the paths inside `LEGO.JAM`. Any file there replaces the original. Path case doesn't matter.
- `music/` works like `<data>/music/`. The music folder wins over every mod.

`mod.toml`:

```toml
id = "hd-bricks"
name = "HD bricks"
version = "1.0.0"
priority = 10
enabled = true
```

| Key | Required | Notes |
|---|---|---|
| `id` | Yes | Must match the folder name. Lowercase letters, digits, `_` and `-` only. |
| `name` | Yes | Shown in the overlay's **Mods** menu |
| `version` | Yes | Text, e.g. `1.0.0` |
| `priority` | No | Whole number, default `0`. Higher wins when two mods replace the same file. On a tie, the id that sorts first wins. |
| `enabled` | No | Default `true`. Sets the starting value of `mods.<id>.enabled`. |

Turning a mod on or off applies on the next start. A mod with a broken `mod.toml` is skipped with a warning in the log.

### Overrides

To replace a few files without writing a mod, put them in `<data>/overrides/` at their paths inside `LEGO.JAM`, e.g. `<data>/overrides/GAMEDATA/COMMON/RRJMW.GDB`.

- Overrides win over every mod. Path case doesn't matter.
- **Loose file overrides** in the **Mods** menu (`mods.overrides`) turns them off. It applies on the next start.
- Replays record which files are overridden and warn when played back with different ones.

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
- [stb_vorbis](https://github.com/nothings/stb) for OGG Vorbis decoding
- [FFmpeg](https://ffmpeg.org/) for the intro videos' Indeo 5 decoder, built from source under the [LGPL-2.1-or-later](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html) with only that decoder enabled
- [Dear ImGui](https://github.com/ocornut/imgui) for the overlay
- [toml++](https://github.com/marzer/tomlplusplus) for settings and mod manifests
- [glm](https://github.com/g-truc/glm) for rendering math
- [doctest](https://github.com/doctest/doctest) for unit tests
- The [LEGO Racers decompilation](https://github.com/isledecomp/racers) by the isledecomp team, used as a reference for game behavior and file formats

## License

[GPL-3.0](LICENSE).

racers-rebuilt isn't affiliated with or endorsed by the LEGO Group. LEGO is a trademark of the LEGO Group.
