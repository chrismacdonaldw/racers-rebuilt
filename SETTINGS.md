# Settings

Every setting lives in `<data>/settings.toml`, in the [data folder](README.md#data-folder), and shows in the <kbd>F1</kbd> overlay.

- Every change saves when an edit finishes.
- Video, audio and enhancement changes apply on the next frame. Game changes apply on the next start, and mod changes on **Apply now** or the next start. Rows below say when a setting differs.
- Items marked * in the overlay change gameplay: replays record them and warn when played back with other values.
- Out-of-range values are clamped. Unknown keys are kept when the file is saved.

```toml
[video]
vsync = false
fps_cap = 240

[audio]
music_volume = 0.6
```

- [Game](#game)
- [Video](#video)
- [Audio](#audio)
- [Camera](#camera)
- [Race](#race)
- [Cheats](#cheats)
- [Modifiers](#modifiers)
- [Mods](#mods)
- [Controls](#controls)
- [Quick config files](#quick-config-files)
- [Command line](#command-line)

## Game

| Key | Default | Values |
|---|---|---|
| `game.version` | `1999` | `1999`, `2001`. The release to play, the original 1999 one or the 2001 re-release. Needs that release's files. Marked * in the overlay. Applies on **Restart now** or the next start. |
| `game.input_lag_2001` | `false` | `true`, `false`. The 2001 release reads the controls after each frame's step, so every frame drives on the controls from the frame before. This adds that lag back as one 12 ms tick. Only works with `game.version` 2001, and is greyed out in the overlay otherwise. Marked * in the overlay. |
| `game.path` | Empty | The LEGO Racers folder of the 1999 release |
| `game.path_2001` | Empty | The LEGO Racers folder of the 2001 release |
| `game.language` | From the original installer, else `ENGLISH` | `ENGLISH`, `GERMAN`, `FRENCH`, `SPANISH`, `ITALIAN`, `DUTCH`, `DANISH`, `NORWEGI`, `SWEDISH` |
| `startup.intro_videos` | `true` | `true`, `false`. Plays the original's three intro videos at launch. Any key skips the second and third. |

`game.version` 2001 also drives as the 2001 game does:

- Faster steering and recentring, with the stick on a squared curve
- Wheels set from the car's shadow size, and a faster turning nose
- Slopes that bounce like floors, a harder hover bank, and hovering on the wheels
- No body roll from drifting
- Computer racers 3% slower on the first circuit
- Racers moving before hazards each frame
- A lap counted each time a car backs over the finish line from the track just past it
- A red border instead of yellow round the Rocket Racer race on the race pick screens

## Video

| Key | Default | Values |
|---|---|---|
| `video.vsync` | `true` | `true`, `false` |
| `video.fps_cap` | `144` | `30` to `360`, or `0` for uncapped. Applies only with VSync off. |
| `video.resolution_scale` | `1.0` | `0.25` to `2.0`. Renders the scene at this fraction of the window size. |
| `video.fullscreen` | `false` | `true`, `false`. Fills the screen without a border, at the desktop's resolution. <kbd>F11</kbd> switches it. |
| `video.stretch_ui` | `false` | `true`, `false`. Stretches the UI's centered 4:3 area to fill the window. |
| `video.widescreen` | `false` | `true`, `false`. Races fill the window and see more to the sides, instead of the original's 4:3 frame. Menus stay 4:3 and fill the side bars with mirrored copies of their backdrops. |
| `video.hud_width` | `Window` | `Window`, `21:9`, `16:9`. With `video.widescreen` on, the race HUD moves out to the window's sides, or stops at the sides of a centred area this wide. **Enhanced** sets `21:9`. |
| `video.widescreen_cutscenes` | `false` | `true`, `false`. With `video.widescreen` on, full-screen cutscenes fill the window instead of the 4:3 frame. |
| `video.smooth_textures` | `false` | `true`, `false`. Gives the game's textures their mipmaps and fills their see-through edges with nearby colours, so distant and filtered edges don't shimmer or show a dark fringe. `false` draws them as the original does, down to the materials the game asks to be drawn with blocky unfiltered pixels. PNG replacements always keep their mipmaps. |
| `enhancements.ldraw_pieces` | `false` | `true`, `false`. Draws car pieces that match LDraw parts with those parts' detailed models, studs included. Applies to the next car loaded or built. |
| `enhancements.ldraw_minifigs` | `false` | `true`, `false`. Beta and very buggy: builds drivers from LDraw minifig parts that keep the original poses, textures and hats, but they can bend, stretch and clip in odd ways. Not part of the Enhanced preset. Applies to the next driver loaded. |
| `enhancements.ldraw_prints` | `false` | `true`, `false`. Draws the matching LDraw minifig torso print over the front of each original driver torso that has one, and redraws original driver faces in every expression in the style of LDraw minifig head prints, at higher resolution. Applies to drivers loaded after it changes. |
| `video.anisotropic_filtering` | `false` | `true`, `false`. Filters textures 16x where they're seen at a slant, so roads and walls stay sharp into the distance. Materials drawn with blocky unfiltered pixels keep them. Applies at once. |
| `video.better_particles` | `false` | `true`, `false`. Replaces the original's tire smoke, drift dust, exhaust, turbo, landing and cannon smoke with new effects: billowing smoke from textures the port makes itself, lit by the sun, growing, turning and fading as it drifts. Crashes, scraped walls and dynamite fuses throw hot sparks that streak, cool and bounce. Every blast and spike explosion bursts into a fireball with a shockwave, embers, lingering smoke and tumbling LEGO bricks. `false` draws the original particles. Applies at once. |
| `video.cockpit_car` | `false` | `true`, `false`. Draws your own car, driver and wheels in the cockpit camera view. `false` leaves them out while racing, as the original does. |

## Audio

| Key | Default | Values |
|---|---|---|
| `audio.music_volume` | `1.0` | `0.0` to `1.0` |
| `audio.effects_volume` | `1.0` | `0.0` to `1.0` |
| `audio.voice_volume` | `1.0` | `0.0` to `1.0` |
| `audio.prevent_clipping` | `false` | `true`, `false`. Mixes with headroom and a limiter so loud moments don't crackle. `false` mixes at full volume and clips, as the original does. |

## Camera

| Key | Default | Values |
|---|---|---|
| `enhancements.free_camera` | `false` | `true`, `false` |
| `enhancements.free_camera_speed` | `40.0` | `1.0` to `400.0` |

With the free camera on:

| Key | Action |
|---|---|
| <kbd>W</kbd> <kbd>A</kbd> <kbd>S</kbd> <kbd>D</kbd> | Move |
| <kbd>Q</kbd> / <kbd>E</kbd> | Down / up |
| Arrow keys | Look |
| <kbd>Shift</kbd> | Move faster |

## Car builder

| Key | Default | Values |
|---|---|---|
| `enhancements.enhanced_builder` | `false` | `true`, `false`. Opens the workshop in place of the original builder: a full-window builder with a free camera, drag and drop, and room to lay bricks on the ground beside the car. The **Enhanced** preset turns it on. Applies straight away, reopening the builder on the same car. |
| `enhancements.extra_pieces` | `false` | `true`, `false`. Adds an **LDraw** tab to the workshop's drawer, with 67 bricks, plates, tiles, slopes and wedges the original never had. They're saved with the car like any piece, and their shapes change how it handles. With it off, a racer whose car has extra pieces can't be picked to race, test driven or have its car saved until they're taken off. The **Enhanced** preset turns it on. Applies straight away. |
| `enhancements.builder_colour` | `#3A6EA5` | A colour as `#RRGGBB`. The workshop's background when it isn't a track. |
| `enhancements.builder_background` | `colour` | `colour`, or a race track's folder, such as `RACEC0R0`. Builds on that track's start line, under its sky. The workshop's **SCENE** button sets this and `builder_colour`. |

In the workshop:

| Input | Action |
|---|---|
| Drag a piece from the drawer | Place it on the car or the ground where you let go |
| Drag a placed piece | Move it on or off the car, or remove it by letting go over the drawer |
| Mouse wheel or <kbd>R</kbd> while dragging | Turn the piece |
| Right-click or <kbd>Esc</kbd> while dragging | Put the piece back |
| Click a placed piece | Select it, or clear the selection with a second click or <kbd>Esc</kbd> |
| Click a swatch, <kbd>1</kbd> to <kbd>0</kbd> | Recolour the selected piece, or build in that colour |
| **TURN**, <kbd>R</kbd> | Turn the selected piece where it is |
| **DELETE**, <kbd>Delete</kbd>, <kbd>Backspace</kbd> | Remove the selected piece |
| **UNDO**, <kbd>Ctrl</kbd>+<kbd>Z</kbd> | Undo a change to the car or the ground |
| **REDO**, <kbd>Ctrl</kbd>+<kbd>Y</kbd>, <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Z</kbd> | Redo |
| Right-drag | Turn the camera |
| Middle-drag, <kbd>Shift</kbd>+right-drag | Pan the camera |
| Mouse wheel | Zoom |
| **FRAME**, <kbd>F</kbd> | Frame the car |
| **SCENE** | Pick a colour or a race track to build on |
| <kbd>Home</kbd> | Reset the camera |
| **DONE**, <kbd>Enter</kbd>, <kbd>Esc</kbd> | Leave the builder |

A piece with others resting on it can't move or come off. The pieces on top flash instead. The chassis never comes off. Bricks on the ground aren't part of the car: they don't count toward its 64 pieces and are gone when you leave.

## Race

These apply from the next race.

| Key | Default | Values |
|---|---|---|
| `race.respawn` | `false` | `true`, `false`. Puts a racer back on the track after it falls off or stays in the air too long. |
| `race.respawn_seconds` | `3` | `1` to `30`. Seconds in the air before a respawn. |
| `race.physics_ai` | `false` | `true`, `false`. Computer racers drive with the car physics instead of replaying their recorded routes. |
| `race.ai_pace` | `100` | `50` to `150`. With `race.physics_ai`, computer racers ease off above this percentage of their route's recorded speed. |
| `race.ai_braking` | `false` | `true`, `false`. With `race.physics_ai`, computer racers brake before their route slows for a corner. |
| `race.ai_passing` | `false` | `true`, `false`. With `race.physics_ai`, computer racers steer around a rival just ahead. |
| `race.ai_unstick` | `false` | `true`, `false`. With `race.physics_ai`, a stuck computer racer backs up before driving on. |
| `race.ai_aiming` | `false` | `true`, `false`. With `race.physics_ai`, computer racers steer to line up shots and drops. |
| `race.ai_skill` | `false` | `true`, `false`. With `race.physics_ai`, computer racers race at a skill level from 1 to 4 instead of `race.ai_pace`. Level 3 drives like a typical boss of the original, and level 4 is Expert. With `race.ai_racing_line`, level 4 also takes the fastest recorded route, shortcuts included. |
| `race.ai_difficulty` | `3` | `1` to `4`. The skill level for `race.ai_skill`. |
| `race.ai_spread` | `Original` | `Original`, `Equal`, `Custom`. Original gives fast-route racers and the boss the level, medium-route racers one less and slow-route racers two less. Equal gives everyone the level. Custom uses each grid slot's level. |
| `race.ai_level_slot1` to `race.ai_level_slot5` | `3` | `1` to `4`. Each grid slot's level when `race.ai_spread` is `Custom`. |
| `race.ai_fair_catch_up` | `false` | `true`, `false`. After 15 s, a skilled racer behind the best player drives one level higher, and one a tenth of a lap ahead drives one level lower. |
| `race.ai_racing_line` | `false` | `true`, `false`. With `race.physics_ai`, computer racers drive a smoothed line inside their route's width, at the fastest speed their car can corner and brake for. `race.ai_pace`, or the skill level, then scales that speed, and they brake for it only with `race.ai_braking`. |
| `race.ai_gate_paths` | `false` | `true`, `false`. With `race.physics_ai`, computer racers drive a path traced from the track's checkpoints and collision instead of the original recordings, with a racing line on top. A track with no recordings at all uses gate paths whatever this is set to, so custom tracks race without recordings. |
| `race.better_warp` | `false` | `true`, `false`. A warp lands a player facing a clear way along the track instead of the next checkpoint, which can point into a wall. |
| `race.fair_warp` | `false` | `true`, `false`. A warp saves about 3 s instead of 5 to 6, for every racer, and a player lands at top speed instead of being launched. |
| `race.catch_up_warp` | `false` | `true`, `false`. The race leader's warp gains a third as much, for every racer. |
| `fixes.stuck_warp` | `false` | `true`, `false`. A warp taken back because 3 are already flying ends properly. In the original, that racer stays a hidden ghost for the rest of the race, which lets a computer racer finish in seconds. |
| `race.opponents` | `5` | `0` to `5`. Computer racers in a single race, from the options screen. |
| `race.versus_laps` | `3` | `1` to `5`. Laps in a versus race, from the options screen. |

## Cheats

`cheats.<plate>` keeps one of the original's license plate cheats on, such as `cheats.mxpmx`. They're entered after the license's own cheats, from the next single race, versus race or test drive. Circuits and time trials ignore them, as the original does, and so does the attract demo.

The other cheats apply to players from the next race of any kind, circuits and time trials too, but not test drives or the attract demo.

| Key | Default | Values |
|---|---|---|
| `cheats.<plate>` | `false` | `true`, `false` |
| `cheats.invincible` | `false` | `true`, `false`. Weapons, hazards and curses can't touch a player's racer, and a shield still works. |
| `cheats.bottomless_brick` | `false` | `true`, `false`. Using a power-up keeps it. |
| `cheats.full_white` | `false` | `true`, `false`. Players always hold 3 white bricks. |
| `cheats.brick_magnet` | `false` | `true`, `false`. Players pick up bricks from 3 times as far. |
| `cheats.long_turbo` | `false` | `true`, `false`. Players' turbo boosts last 3 times as long. |
| `cheats.perfect_start` | `false` | `true`, `false`. Players get the strong start boost at every go, with no timing. |
| `cheats.free_warp` | `false` | `true`, `false`. Every green power-up a player uses warps, whatever the white bricks. |
| `cheats.head_start` | `0` | `0` to `10`. Seconds the computer racers wait after the go. |
| `cheats.boost_button` | `false` | `true`, `false`. The `boost` button fires a level 1 turbo at any time during the race. |
| `cheats.slow_motion` | `false` | `true`, `false`. Holding the `slow_motion` button runs the race at half speed. Takes effect at once. |
| `cheats.skip_ahead` | `false` | `true`, `false`. The `skip_ahead` button moves a player to the next checkpoint, keeping their speed. |

## Modifiers

Modifiers apply to all racers from the next race of any kind, circuits and time trials too, but not test drives or the attract demo.

| Key | Default | Values |
|---|---|---|
| `modifiers.gravity` | `100` | `25` to `100`. Gravity as a percentage, for floatier jumps. |
| `modifiers.speed` | `100` | `100` to `200`. Every racer's top speed and acceleration as a percentage. |
| `modifiers.mirror` | `false` | `true`, `false`. Mirrors every track. |
| `modifiers.only_powerup` | `off` | `off`, `cannonball`, `grappling_hook`, `lightning`, `missile`, `oil_slick`, `dynamite`, `magnet`, `curse`, `shield`, `turbo`, `warp`. Every use fires this power-up. A shield keeps the held white count, and a turbo up to 2. |
| `modifiers.chaos_bricks` | `false` | `true`, `false`. Every coloured brick picked up becomes a random colour with 0 to 3 white bricks. |
| `modifiers.squeaky_voices` | `false` | `true`, `false`. Drivers' voices and taunts play an octave up. |
| `modifiers.rocket_rush` | `false` | `true`, `false`. Every computer racer is Rocket Racer. |

## Mods

| Key | Default | Values |
|---|---|---|
| `mods.<id>.enabled` | The mod's `enabled` value | `true`, `false` |
| `mods.hq_textures` | `true` | `true`, `false`. Uses PNG replacements for game images. |
| `mods.gltf_models` | `true` | `true`, `false`. Uses glTF replacements for game models and animations. |
| `mods.overrides` | `true` | `true`, `false`. Loads `<data>/overrides/`. |

Once a setting in **Mods** changes, two items appear there:

- **Apply now** remounts the mods and overrides, reloads the music, and returns to the main menu. It isn't offered while recording or playing a replay.
- **Restart now** relaunches the game with the same command line, minus `--record`, `--replay`, `--import-save`, `--dump-images`, `--dump-textures`, `--export-gltf` and `--dump-powerups`.

A newly added mod folder, or a change to the menu cursor, still needs a restart.

The mods are listed in priority order, each with its version. A mod that's on but can't load shows why below its name. Turning a mod on also turns on what it requires, and turning off a mod that others require, or turning on one that conflicts with another, asks first. See [Dependencies](MODDING.md#dependencies).

## Controls

| Key | Default | Values |
|---|---|---|
| `input.keys.<action>` | See below | The first keyboard set (arrows). Comma-separated SDL key names, such as `Up,W` or `Left Shift` |
| `input.keys2.<action>` | See below | The second keyboard set (WASD), in the same form |
| `input.keys3.<action>` | See below | The third keyboard set (keypad), in the same form |
| `input.buttons.<action>` | See below | The first gamepad's set. Comma-separated SDL gamepad names: buttons such as `a` or `dpleft`, or `lefttrigger` and `righttrigger` |
| `input.pad2.buttons.<action>` | The first gamepad's defaults | The second gamepad's set, in the same form |
| `input.p1.device` | `pad1` | `pad1`, `pad2`, `keys1`, `keys2`, `keys3`. The set player 1 drives with |
| `input.p2.device` | `pad2` | The set player 2 drives with, in the same form |
| `input.p1.any_device` | `false` | `true` lets every keyboard set and gamepad drive player 1 when racing alone |

Each `<action>` has a default in every set. The sets follow the original's. The left stick always steers, and the menus take every keyboard and gamepad.

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
| `boost` | `B` | `R` | `Keypad 1` | `leftstick` |
| `slow_motion` | `N` | `F` | `Keypad 0` | |
| `skip_ahead` | `J` | `C` | `Keypad 5` | |
| `menu_up` | `Up` | | | `dpup` |
| `menu_down` | `Down` | | | `dpdown` |
| `menu_left` | `Left` | | | `dpleft` |
| `menu_right` | `Right` | | | `dpright` |
| `menu_confirm` | `Return,Space` | | | `a` |
| `menu_back` | `Escape,Backspace` | | | `b` |

- Each player drives with one set, as the original does, so with a gamepad attached player 1 drives with the gamepad alone.
- A set naming a gamepad that isn't attached falls back to the first attached gamepad not in use, then to the arrows, or WASD when the other player has the arrows. Gamepads are numbered in the order they connected.
- A key in both players' sets drives player 1, and the pause key of a set nobody drives still pauses.
- PLAYER 1 CONTROLS and PLAYER 2 CONTROLS on the options screen pick each player's set from the attached gamepads and the three keyboard sets, and rebind it. A gamepad's steering rows stay closed, as it steers with its stick and d-pad. Picking the other player's set moves them to another, as the original does.
- The overlay's **Bindings** menu picks each player's device and shows every keyboard and pad binding in one table.

## Quick config files

Each config saved from **Enhancements → Presets** is a file in `<data>/presets/`, so you can share it. Settings it doesn't list take their defaults, and settings it can't set are ignored:

```toml
name = "Race night"

[settings]
"video.smooth_textures" = true
"race.respawn" = false
"mods.hd-tracks.enabled" = true
```

## Command line

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
| `--dump-textures <folder>` | Write every image in `LEGO.JAM` as a PNG texture pack and exit. See [Make a texture pack](MODDING.md#make-a-texture-pack). |
| `--export-gltf <folder>` | Write every model in `LEGO.JAM` as a `.glb` and exit. See [Models](MODDING.md#models). |
| `--dump-powerups <file>` | Write every power-up value as a `POWERUPS.TOML` and exit. See [Power-ups](MODDING.md#power-ups). |
| `--check-mods` | Check every mod and exit. Each problem is logged with its `mod.toml`, and a message lists them all. See [Dependencies](MODDING.md#dependencies). |

```powershell
.\racers-rebuilt.exe --set video.vsync=false --set video.fps_cap=60
```
