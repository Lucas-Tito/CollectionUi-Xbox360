# CollectionUI for Xbox 360

A game collection browser for modded Xbox 360 consoles, in the spirit of Steam's Big Picture
collections. It runs as a homebrew app next to FreeStyle Dash, reads the library FreeStyle has
already scanned, and lets you group games into collections you build yourself. You browse them
with the controller and launch games from there.

The interface follows the console's own Metro dashboard: hubs across the top, square collection
tiles, cover cards and a details panel that slides up from the bottom.

## Features

- **Your own collections.** Three kinds:
  - **Manual**: games you pick one by one.
  - **Union**: everything in any of the chosen collections.
  - **Intersection**: only what is in all of them. "Co-op" crossed with "Not finished" gives
    what you can play together and haven't completed.
- **Category tabs** (LB/RB) from FreeStyle's own categories: XBLA, Xbox 360, Xbox OG, Homebrew,
  Emulators. A tab narrows what every collection shows, without changing what the collection
  holds.
- **Unsorted.** The first tab lists the games that aren't in any collection yet. There, X files a
  game into several collections at once.
- **Game details** (Y). Couch co-op from Co-Optimus, release year, developer, publisher,
  Metacritic, How Long to Beat and a description, when that data is available.
- **Launches** retail, XBLA and arcade games, homebrew and emulator ROMs.
- **Export to Xbox Vault.** Writes `vault.txt` with everything on the console (games, ROMs, apps
  and collections), for the [xbox-vault](https://github.com/Lucas-Tito/xbox-vault) catalog.

## Requirements

- An Xbox 360 that runs unsigned code (RGH or JTAG).
- **FreeStyle Dash 3**, with your games already scanned. CollectionUI reads FreeStyle's
  `content.db` and cover art; it does not scan on its own.

It is not a dashboard replacement. It opens like any other title and doesn't come back after
launching a game.

## Installing

1. Download `CollectionUI-vX.Y.Z.zip` from [Releases](../../releases).
2. Copy the `CollectionUI` folder to the console, for example to `Hdd1:\HOMEBREW\` (or wherever
   your FreeStyle looks for homebrew).
3. Launch it from FreeStyle.

Your collections are saved in `colecoes.txt`, next to the `.xex`. The file is created on first use,
and copying a new version over the old one leaves it alone.

### Optional: data for the details panel

The details panel reads `fichas.txt` from the same folder. Without that file the panel uses what
FreeStyle stores (genre, developer, publisher, release date, rating), and games with nothing to
show don't offer Y at all.

To generate `fichas.txt`:

1. Export from the app (Back on the collections screen) to get `vault.txt`.
2. Run [`ferramentas/fichas.py`](ferramentas/fichas.py) with the xbox-vault data.

## Controls

**Collections**

| button | action |
|---|---|
| A | open the collection |
| X | new collection |
| Start | options: rename, choose the source collections, delete |
| Back | export to Xbox Vault |
| LB / RB | change tab |

**Inside a collection**

| button | action |
|---|---|
| A | play |
| Y | details |
| X | add games (manual collections) |
| Start | options: remove the game from the collection |
| B | back |
| LB / RB, or hold up/down | jump to the next or previous letter |

**Unsorted**

| button | action |
|---|---|
| A | play |
| Y | details |
| X | add to collections |
| B | back to the collections screen |

## Building from source

The app is C++ against the Xbox 360 XDK and builds on Linux. The XDK's compiler runs under wine in
a Docker image. The XDK itself is proprietary and not included: you need your own copy.
[`toolchain/README.md`](toolchain/README.md) has the full setup.

```bash
./toolchain/build.sh app CONFIG=Release   # -> app/build/Release/bin/collectionui.xex
```

To make a release package, put the `.xex` and `app/media/` in a `CollectionUI/` folder.

## Repository layout

| path | what's there |
|---|---|
| `app/` | the app: `main.cpp` (screens, drawing, input) and `src/` (library, collections, launching, emulators, export, details data) |
| `toolchain/` | Docker image and scripts that build the `.xex` on Linux |
| `previa/` | a PC preview that draws the console screen in a browser canvas, to check changes before copying them to the console |
| `ferramentas/` | developer tools: automated screenshots over xbdm, `fichas.txt` generation, ROM covers |
| `docs/` | [`technical-notes.md`](docs/technical-notes.md) (data sources, file formats, launching) and [`design-decisions.md`](docs/design-decisions.md) (what was decided and why) |

## Status

Developed and tested on a real console. Known gap: indie games launch through the system XNA launcher, the
same way FreeStyle does it, but this hasn't been tested on hardware yet.
