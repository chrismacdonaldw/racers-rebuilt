# Modding

Music, mods and loose overrides all live in the [data folder](README.md#data-folder). The overlay's **Mods** menu turns them on and off, and [Mods settings](SETTINGS.md#mods) says when changes apply.

## Music

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

A mod with a broken `mod.toml` is skipped with a warning in the log.

### Dependencies

`mod.toml` can name other mods by id, each with a version range, in three optional tables:

```toml
[requires]
"brick-pack" = ">=1.2.0"

[after]
"hd-bricks" = "*"

[conflicts]
"old-tracks" = "*"
```

| Table | Meaning |
|---|---|
| `requires` | The mod only loads when each named mod is installed, on and in range. Turning the mod on in the **Mods** menu also turns those on. |
| `after` | If the named mod is on and in range, it loads first. Nothing happens otherwise. |
| `conflicts` | The two can't both load. With both on, the higher priority one loads. Turning one on in the **Mods** menu asks which to keep. |

| Range | Matches |
|---|---|
| `*` | Any version |
| `=1.2.0` | Only `1.2.0` |
| `>=1.2.0` | `1.2.0` or newer |
| `^1.2.0` | `1.2.0` or newer with the same first number, so not `2.0.0` |

- Versions compare as `x.y.z` whole numbers. A `version` in another form only matches `*`.
- A mod named in `requires` or `after` always loads below the mod naming it, whatever their priorities. Otherwise priority decides, then the id.
- A mod that can't load stays on but is held back. The **Mods** menu shows why under its name, e.g. "Needs brick-pack 1.2.0 or newer", and the log says it too.
- Mods whose `requires` and `after` form a loop are all held back, with the mods they loop through.
- Applying a quick config turns on what its mods require.
- Replays record each loaded mod's id, version and a hash of its file names and sizes, and warn when played with different ones.
- `racers-rebuilt.exe --check-mods` checks every mod against the current settings, lists each broken `mod.toml` and held-back mod with its reason, and exits.

## Tracks

A mod adds a race track with a new folder in `files/GAMEDATA/`. Any folder there with a `STARTPOS.SPB` and a `RACE.CPB` is a track. It shows at the end of the single race and versus track lists under its folder name, in the overlay's **Scenes** menu, and `--race <folder>` starts a race on it.

Starting from a copy of a retail folder such as `RACEC0R0`, rename the files named after it:

| Retail file | New track `<NAME>` |
|---|---|
| `RACEC0R0.TMB`, `.RAB`, `.EMB`, `.TGB` | `<NAME>.TMB`, `.RAB`, `.EMB`, `.TGB` |
| `LEVC0R0.SBK` | `<NAME>.SBK` |

Keep the `EMITC0R0` files as they are: the track's worlds refer to them by name.

New tracks aren't part of any circuit, so they race with the first circuit's grid.

### Track info

A `TRACK.TOML` in the track's folder sets how the menus show it and how it races. Every key is optional.

```toml
name = "Lava Lane"
preview = "PREVIEW.png"
laps = 5
grid = "c2"
time_trial = true
mirrored = true
music = ["lavalane", "circuit3"]
```

- `name`: the name in the track lists and the **Scenes** menu, instead of the folder name.
- `preview`: a PNG in the track's folder, shown in the race setup preview box.
- `laps`: 1 to 9. Single races use it. Versus keeps its own lap setting and time trials stay at 3 laps.
- `grid`: the circuit whose racers line up, `c0` to `c6`.
- `time_trial`: lists the track in time trials too.
- `mirrored`: adds a mirrored copy to the single race and versus lists.
- `music`: [music](#music) ids. The list replaces the race music: the first plays as the race music and the rest are picked from at random. The intro, lose and win music stay as they are.

A `TRACK.TOML` in an original track's folder only changes its `music` and its name in the **Scenes** menu.

`--check-mods` also reports a broken `TRACK.TOML`, a `grid` that names no circuit and a missing `preview`.

## Circuits

A mod adds or changes circuits with `files/MENUDATA/CIRCUITS.TOML`. Mods' tables apply in load order.

```toml
[[circuit]]
name = "lava-cup"
title = "LAVA CUP"
boss = "MG"
grid = ["VV", "MG", "LV", "SP", "BO", "JK"]
races = ["LAVALANE", "RACEC2R1", { track = "LAVALANE", mirrored = true }]
after = "c2"
part_set = "lava-parts"
intro = "circuit3"
```

- `name`: lowercase letters, digits, `-` and `_`. An existing name such as `c3` changes that circuit: only the keys given replace its own, and `races` replaces all of its races.
- `after`: puts a new circuit into the story after the one named. Gold or silver there opens the new circuit, and gold or silver on the new circuit opens what used to come next. Circuits put after the same one keep load order. A new circuit is also open once any later circuit is, so players already past it can race it.
- `title`: the name in the circuit list.
- `grid`: driver codes for grid slots 0 to 5, as in the original circuits. Slot 1 is the boss unless `boss` in the same table names one.
- `boss`: the champion shown in the circuit list, on the award screens and on the main menu.
- `races`: 1 to 4 track folders in race order. A table with `mirrored = true` races the track mirrored. On an original circuit, its old tracks leave Single Race.
- `part_set`: a mod part set won with gold. It's only recorded in `progress.toml` under `part_sets_named` for now, since mod part sets are always available in the builder.
- `intro`: an original intro screen, `circuit1` to `circuit7`, or `""` for none. A new circuit has none unless it names one.

A new circuit needs `after`, `title`, `grid` and `races`. A change that can't apply is left out with a warning in the log, and `--check-mods` lists it with any race or driver that doesn't exist.

## Racers, driver parts and bricks

These tables merge with the game's instead of replacing it, so a mod only needs the entries it adds or changes:

| File in `files/` | Entries |
|---|---|
| `GAMEDATA/COMMON/DRIVERS.DDB` | Opponent drivers, by code |
| `GAMEDATA/COMMON/CHAMPS.CCB` | Champion cars, by name |
| `GAMEDATA/COMMON/CHASSIS.CMB`, `MENUDATA/PIECEDB/CHASSIS.CMB` | Chassis, by name |
| `MENUDATA/RACERS.TOML` | Prebuilt racers in the racer pick, by name |
| `MENUDATA/PARTDB/BODYPART.PCB` | Hats, faces, torsos and legs, by name. A hat brings its head part with it |
| `MENUDATA/PARTDB/MENUPART/CBBODIES.GCB`, `MENUDATA/PARTDB/GAMEPART/ICB_CHAR.GCB` | Head parts, by name, with their own vertices |
| `MENUDATA/PIECEDB/LPIECEHI.LEB`, `MENUDATA/PIECEDB/LPIECELO.LEB` | Builder bricks, by name, with their own vertices. Their materials come from the `.MDB` beside them in the same mod |
| `MENUDATA/PIECEDB/CRSTMGR.LEB` | Part set files. A mod's sets come after the game's 12, are always available, and show the icon of the game set with their chassis |
| Any `.MDB` or `.TDB` | Materials and textures, by name |

- An entry named like one before it, in any case, replaces it. Mods merge in priority order, lowest first, and overrides last.
- Saved racers store each driver part's name beside its position (`face_name`, `hat_name`, `torso_name` and `legs_name` in `racers.toml`). The name wins, so turning mods on or off never swaps a racer's parts. A racer saved before names were stored keeps its positions until its driver is edited.
- A racer wearing a part from a mod that's off is kept but can't race or test drive, and shows "Needs parts from a mod". Until then it wears the part at the saved position, or the first part. Turning the mod back on unlocks it, and editing the driver keeps the missing part unless it's changed.
- Each list of hats, faces, torsos or legs holds 256 parts. Parts past that are skipped with a warning.
- A mod's table that doesn't load is skipped with a warning in the log.
- A new brick needs a piece type of `0x1389` or more that no other brick uses, since saved cars store the type. A brick named like a game brick must keep its type. Bricks that break either rule are skipped with a warning, and a car saved with a mod's brick loses it once the mod is off.
- `RACERS.TOML` uses the data folder's `racers.toml` layout, so a racer built in the game can be copied in, and its racers can name their parts. Trophies and `selected` are ignored.
- Entries can't be removed.

## Driver parts

`files/MENUDATA/PARTDB/PARTS.TOML` adds hats, faces, torsos and legs without binary tables. Paths are relative to `MENUDATA/PARTDB`:

```toml
[[hat]]
name = "lava-helmet"
model = "lava/helmet.glb"

[[colour]]
name = "lava_red"
rgb = [200, 60, 20]

[[face]]
name = "lv_"
default = "lava/face.png"
happy = "lava/happy.png"

[[torso]]
name = "lava-suit"
texture = "lava/suit.png"
hook = true

[[legs]]
name = "lava-legs"
texture = "lava/legs.png"
peg = false
```

- A hat's `.glb` is the whole head, hat included, as `--export-gltf` writes the game's heads under `MENUDATA/PARTDB/MENUPART/CBBODIES/`. The material named `face` shows the driver's face. Other materials name a game material from `BODYPART.MDB` or a `[[colour]]`. Menus and races use the same model.
- A face needs only `default`. `angry`, `blink`, `happy`, `sad` and `surprised` each fall back to it. A face's `name` is its material prefix, as in `BODYPART.PCB`.
- Torso and leg textures replace the image of the first torso or legs. `hook = true` gives hook hands, and `peg = true` a peg leg.
- Images are PNG. A face, torso or legs image copies the first part's material and texture settings.
- Parts are always available, and come after the game's and `BODYPART.PCB`'s parts. A part named like one before it replaces it. Each mod's file merges in priority order, lowest first.
- A broken entry, or an image or model that doesn't load, is skipped with a warning in the log.

## Power-ups

`files/GAMEDATA/COMMON/POWERUPS.TOML` changes how power-ups behave. A mod only lists the keys it changes:

```toml
[missile]
arm_ms = [933, 1200, 1333]
range = 500.0

[slots]
yellow = ["oil_slick", "dynamite", "magnet", "lightning"]
```

- Every mod's file applies over the original values, in priority order, lowest first. The same key in a later mod wins.
- `racers-rebuilt.exe --dump-powerups <file>` writes every key with its original value, then exits. It's a good place to start.
- Values are read when a race starts and stay fixed until it ends.
- A value outside its range is clamped, and an unknown key, a wrong type or a list of the wrong length is skipped. Each gets a warning in the log. A file that doesn't parse is skipped whole.
- Changed values are part of a replay's fingerprint, so an old replay warns when they differ.

### Slots

`[slots]` sets what each brick colour gives at 0 to 3 white bricks. Its keys are `red`, `blue`, `green` and `yellow`, each a list of 4 of: `cannonball`, `grappling_hook`, `lightning`, `missile`, `oil_slick`, `dynamite`, `magnet`, `curse`, `shield`, `turbo`, `warp`.

- The HUD shows the icon of the power-up the slot gives, tinted in the brick's colour.
- Hazards, the start boost and the warp cheat fire the original power-ups.
- Computer racers still decide by colour, so they use a remapped slot the way they'd use that colour.

### Keys

All `_ms` keys are whole milliseconds, from 1 to 600,000. Distances are in game units.

Shots (`cannonball`, `grappling_hook`, `missile`, `dynamite`) share three keys:

| Key | Unit | Range |
|---|---|---|
| `speed` | units/s | 1 to 2,000 |
| `gravity` | units/s², down is negative | -1,000 to 0 |
| `lifetime_ms` | ms | |

A cone `<name>` has three keys. A racer or target point is in it when its distance is between `<name>_min` and `<name>_max` (units, 0 to 2,000), and the cosine of its angle from the car's heading is at least `<name>_cosine` (-1 to 1).

| Section | Key | Unit | Original |
|---|---|---|---|
| `cannonball` | `speed`, `gravity`, `lifetime_ms` | shot | 180, -32.176, 3000 |
| | `hazard_speed`, `hazard_gravity`, `hazard_lifetime_ms` | a cannon hazard's shot | 30, -96.528, 3000 |
| | `racer_cone` | cone | 10 to 400, 0.9 |
| | `target_cone` | cone, for track target points, tried first | 10 to 400, 0.95 |
| | `range` | units ahead to aim with no target | 500 |
| `grappling_hook` | `speed`, `gravity`, `lifetime_ms` | shot | 320, -90.176, 3000 |
| | `far_cone` | cone, tried first, for the farthest racer | 10 to 250, 0.93 |
| | `near_cone` | cone, for the nearest racer | 10 to 400, 0.6 |
| | `target_cone` | cone, for track target points, tried last | 10 to 400, 0.95 |
| | `range` | units | 500 |
| | `pull_ms`, `retract_ms` | ms | 4000, 500 |
| | `pull_force` | units/s², 0 to 10,000 | 180 |
| | `release_distance` | units, lets go when closer | 12 |
| | `chase_speed`, `retract_speed` | units/s, 1 to 2,000 | 300, 250 |
| | `tension_per_second` | 0 to 100, the rope checks for walls once tension reaches 1 | 1 |
| `lightning` | `ramp_ms`, `sustain_ms`, `fade_ms` | ms | 500, 7000, 500 |
| | `hold_ms` | ms a struck racer stays held | 1000 |
| | `segment_length` | units per bolt segment, 1 to 500 | 50 |
| | `victim_cone` | cone | 3 to 200, 0.5 |
| | `hit_strength` | 0 to 10 | 1 |
| `missile` | `speed`, `gravity`, `lifetime_ms` | shot | 170, -32.176, 5500 |
| | `arm_ms` | list of 3, one per missile | 933, 1200, 1333 |
| | `side_offsets` | list of 3, units right of the aim with no target, -2,000 to 2,000 | 150, 0, -150 |
| | `range` | units | 500 |
| | `spin_turns` | turns, 0 to 10 | 2 |
| | `spin_rate_per_ms` | 0.0001 to 1 | 0.007 |
| `oil_slick` | `radius` | units | 3 |
| | `lifetime_ms` | ms | 10000 |
| | `spin_turns`, `spin_rate_per_ms`, `spin_align` | as `missile`; `spin_align` 0 to 10 | 1, 0.007, 1 |
| `dynamite` | `speed`, `gravity`, `lifetime_ms` | shot | 40, -32.176, 3000 |
| | `target_cone` | cone, behind the car | 10 to 400, 0.9 |
| | `range` | units behind to land with no target | 90 |
| | `fuse_ms`, `blast_interval_ms` | ms | 5000, 500 |
| | `scatter` | whole units, 0 to 100, for later blasts | 6 |
| `magnet` | `radius` | units | 10 |
| | `armed_ms`, `holding_ms`, `fade_ms`, `halted_hold_ms` | ms | 20000, 4000, 1000, 1000 |
| | `grab_distance`, `lift_stop_distance` | units | 9, 10 |
| | `stopped_speed` | units/ms, 0 to 100 | 0.002 |
| | `pull_per_ms` | units/ms, 0 to 100 | 1 |
| `curse` | `radius` | units | 10 |
| | `active_ms`, `fade_ms`, `curse_ms` | ms | 15000, 1000, 10000 |
| `shield` | `durations_ms` | list of 4, one per white brick count | 4000, 6000, 8000, 10000 |
| | `fade_ms` | ms | 1000 |
| | `deflect_level` | white bricks from which shots turn back, 0 to 4 | 2 |
| `turbo` | `levels` | list of 4 of `weak`, `strong`, `hover` | weak, strong, hover, hover |
| `warp` | `starting_ms`, `active_ms` | ms | 500, 1500 |
| | `walk_per_ms` | units/ms, 0.01 to 10 | 0.6 |
| | `route_speed` | a computer racer's route speed, 0.1 to 50 | 4.5 |

Distances and ranges not listed run from 0 to 2,000 units.

## HQ textures

A PNG next to a game image's path, named like it but ending in `.png`, replaces that image, e.g. `files/GAMEDATA/COMMON/BRICK.png` for `GAMEDATA/COMMON/BRICK.TGA`.

- It can be any size. Menus and the HUD lay it out at the original image's size.
- Its alpha is used as is: the original's colour key doesn't apply.
- **HQ textures** in the **Mods** menu (`mods.hq_textures`) turns replacements off. It applies on **Apply now** or the next start.
- A torso or face PNG or BMP from a mod wins over **LDraw driver prints** (`enhancements.ldraw_prints`), which draws LDraw torso prints onto some drivers' torsos and redraws original driver faces.

## Make a texture pack

1. Run `racers-rebuilt.exe --dump-textures <data>/mods/my-pack`. It writes every image in `LEGO.JAM` as a PNG under `my-pack/files/`, named for the image it replaces, plus a `mod.toml`, then exits. It won't overwrite a folder that already has a `mod.toml`.
2. Edit or upscale the PNGs, e.g. with an upscaler such as Real-ESRGAN. Any size works.
   - Keep the alpha channel. Colour-keyed images are dumped with their key already transparent and the see-through pixels filled with nearby colours, so edges don't pick up the key colour. Upscale the alpha separately if the tool supports it.
   - Pad textures that tile before upscaling, then crop, to avoid seams.
3. Delete the PNGs you didn't change, so the originals still load for those.
4. Turn the pack on in the **Mods** menu: it starts disabled. **HQ textures** turns every pack's PNGs off at once.

## Models

A glTF binary next to a game model's path, named like it but ending in `.glb`, replaces that model, e.g. `files/GAMEDATA/COMMON/RRJMW.glb` for `GAMEDATA/COMMON/RRJMW.GDB`.

- Export it as **glTF Binary** with Y up, as Blender does by default. Node transforms are applied.
- Each material's name picks the game material it's drawn with, so name it after one in the materials file (`.MDB`) the game loads with that model. Case doesn't matter.
- Every mesh needs normals or vertex colours. Vertex colours make the whole model unlit, the way the game draws coloured models.
- Units are the game's drawn units: the original model's scale is kept.
- A model with a skeleton (an `.SDB` of the same name, or `PELVIS.SDB` for drivers) is jointed. Name glTF nodes after the skeleton's nodes (case doesn't matter) and place them where the game's skeleton has them, since the game moves its own nodes.
  - A mesh belongs to the nearest named node at or above it, else to the skeleton's first node, and is stored relative to that node.
  - A skinned mesh gives each vertex the joint it's weighted to most. Every skin joint must be named after a skeleton node.
- A `.glb` next to an animation file (`.ADB`) replaces its animations too, e.g. `PELVIS.glb` for the drivers' shared `PELVIS.ADB`. A model's `.glb` also replaces the animations of an `.ADB` with the same name, so actions in it that share a name with a game animation take effect.
  - Each glTF animation (action) replaces the game animation with the same name (case doesn't matter). The others are kept.
  - Rotations and moves of nodes named after the skeleton's nodes are used. Nodes and channels the animation doesn't key keep the game animation's keys. Linear and step keys work; cubic spline keys don't.
  - Keys snap to the game animation's frame rate. The animation's speed, order and bounds are kept, so a part swung far outside the original's bounds may be culled.
  - At most 4,000,000 keys are read from one file.
- A `.glb` that can't be read is skipped with a warning in the log, and the game's model or animations are kept.
- **glTF models** in the **Mods** menu (`mods.gltf_models`) turns replacements of both models and animations off. It applies on **Apply now** or the next start.

To start from the game's own models, run `racers-rebuilt.exe --export-gltf <folder>`. It writes every model in `LEGO.JAM` as a `.glb` at its game path under that folder, e.g. `<folder>/GAMEDATA/COMMON/RRJMW.GLB`, and each driver head in a folder named after its head file such as `<folder>/MENUDATA/PARTDB/MENUPART/CBBODIES/`, then exits.

- Materials keep the game's names but carry no textures.
- A jointed model is one skinned mesh in its rest pose, with nodes named after the skeleton's, and its animations as glTF animations of the same names. Driver parts carry the shared `PELVIS.ADB` animations, which only `PELVIS.glb` replaces.
- Copy the `.glb` files you change into a mod's `files` folder at the same paths.

## Brick models

A model at `MENUDATA/PIECEDB/MODELS/<brick name>.GDB`, or a `.glb` there, replaces how a builder brick is drawn on cars, e.g. `files/MENUDATA/PIECEDB/MODELS/L300100.glb`. Where the brick fits and what it hits are unchanged.

- Units are studs: one cell per unit along x and y, and a plate is 0.4 high. Model the brick unturned, with its first cell's corner at the origin and its bottom at z 0. The game turns and places it.
- A material named `piece` (case doesn't matter) takes the colour the brick is placed in. Other names pick a material from `LPIECEHI.MDB`.
- The model draws its own studs: the game adds none on the brick's cells.
- **LDraw pieces** (`enhancements.ldraw_pieces`) draws the bricks, minifig accessories and other car pieces that match parts in the shipped LDraw library this way. A mod's model wins over it.
- The same setting rebuilds the driver bodies in `MENUDATA/PARTDB/MENUPART` and `GAMEPART`, and the heads in their `.GCB` libraries, from LDraw minifig parts. A mod's body `.GDB` or head library wins over it.

## Overrides

To replace a few files without writing a mod, put them in `<data>/overrides/` at their paths inside `LEGO.JAM`, e.g. `<data>/overrides/GAMEDATA/COMMON/RRJMW.GDB`.

- Overrides win over every mod. Path case doesn't matter.
- **Loose file overrides** in the **Mods** menu (`mods.overrides`) turns them off. It applies on **Apply now** or the next start, which also pick up changed files.
- Replays record which files are overridden and warn when played back with different ones.
