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

## Tracks

A mod adds a race track with a new folder in `files/GAMEDATA/`. Any folder there with a `STARTPOS.SPB` and a `RACE.CPB` is a track. It shows at the end of the single race and versus track lists under its folder name, in the overlay's **Scenes** menu, and `--race <folder>` starts a race on it.

Starting from a copy of a retail folder such as `RACEC0R0`, rename the files named after it:

| Retail file | New track `<NAME>` |
|---|---|
| `RACEC0R0.TMB`, `.RAB`, `.EMB`, `.TGB` | `<NAME>.TMB`, `.RAB`, `.EMB`, `.TGB` |
| `LEVC0R0.SBK` | `<NAME>.SBK` |

Keep the `EMITC0R0` files as they are: the track's worlds refer to them by name.

New tracks aren't part of any circuit, so they race with the first circuit's grid.

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
- New entries come after the game's, so saved racers keep their parts. A racer saved with a mod's part gets the first part once the mod is off.
- Saved racers store a part's position in its list, so turning off one mod can give a racer the part of a mod after it.
- Each list of hats, faces, torsos or legs holds 256 parts. Parts past that are skipped with a warning.
- A mod's table that doesn't load is skipped with a warning in the log.
- A new brick needs a piece type of `0x1389` or more that no other brick uses, since saved cars store the type. A brick named like a game brick must keep its type. Bricks that break either rule are skipped with a warning, and a car saved with a mod's brick loses it once the mod is off.
- `RACERS.TOML` uses the data folder's `racers.toml` layout, so a racer built in the game can be copied in. Trophies and `selected` are ignored.
- Entries can't be removed.

## HQ textures

A PNG next to a game image's path, named like it but ending in `.png`, replaces that image, e.g. `files/GAMEDATA/COMMON/BRICK.png` for `GAMEDATA/COMMON/BRICK.TGA`.

- It can be any size. Menus and the HUD lay it out at the original image's size.
- Its alpha is used as is: the original's colour key doesn't apply.
- **HQ textures** in the **Mods** menu (`mods.hq_textures`) turns replacements off. It applies on **Apply now** or the next start.

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

To start from the game's own models, run `racers-rebuilt.exe --export-gltf <folder>`. It writes every model in `LEGO.JAM` as a `.glb` at its game path under that folder, e.g. `<folder>/GAMEDATA/COMMON/RRJMW.GLB`, then exits.

- Materials keep the game's names but carry no textures.
- A jointed model is one skinned mesh in its rest pose, with nodes named after the skeleton's, and its animations as glTF animations of the same names. Driver parts carry the shared `PELVIS.ADB` animations, which only `PELVIS.glb` replaces.
- Copy the `.glb` files you change into a mod's `files` folder at the same paths.

## Overrides

To replace a few files without writing a mod, put them in `<data>/overrides/` at their paths inside `LEGO.JAM`, e.g. `<data>/overrides/GAMEDATA/COMMON/RRJMW.GDB`.

- Overrides win over every mod. Path case doesn't matter.
- **Loose file overrides** in the **Mods** menu (`mods.overrides`) turns them off. It applies on **Apply now** or the next start, which also pick up changed files.
- Replays record which files are overridden and warn when played back with different ones.
