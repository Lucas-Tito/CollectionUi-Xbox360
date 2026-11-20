# Technical notes

How CollectionUI gets its data, what it reads and writes on the console, how it launches games,
and the tools used to develop it. Identifiers in Portuguese (`colecoes.txt`, `fichas.txt`, `JOGO`,
`COLECAO`...) are literal file names and keys, so they are kept as they are and explained here.

## Requirements

- **A console that runs unsigned code permanently: RGH or JTAG.** The app is a plain `.xex` that
  runs under the Xbox kernel and calls into `xam.xex`, which is what makes launching games possible.
- **FreeStyle Dash 3** installed on the hard drive or a USB drive. CollectionUI does not scan your
  drives for games: it reads the library FreeStyle has already built (database and cover art). It
  is opened from FreeStyle's application list, like any other app, and is not a dashboard
  replacement. Leaving a game does not return to CollectionUI.
- Aurora and other dashboards are not supported as a data source; they use different formats.
- The software-only BadUpdate exploit also runs unsigned code, but it is not persistent and has
  not been tested with this app.

## Building and installing

```bash
./toolchain/build.sh app                 # Debug
./toolchain/build.sh app CONFIG=Release  # Release
```

The result is `app/build/<CONFIG>/bin/collectionui.xex`. Debug links the debug D3D runtime, which
validates calls and makes the binary several times larger.

To install, copy `collectionui.xex` **and the `app/media/` folder** (font atlas, shader, sounds)
into one folder on the console, for example over FreeStyle's built-in FTP server, and open it from
FreeStyle's application list. Everything the app writes goes into that same folder (`game:\`).

## Where the data comes from

### Finding the FreeStyle installation

At startup the app mounts `Hdd:`, `Usb0:`, `Usb1:` and `Usb2:` and looks for
`<device>\<any top-level folder>\Data\Databases\content.db`. If exactly one is found, its path is
saved as `banco=` in `collectionui.ini`. If several are found and none is saved, the app has no
picker: write `banco=Hdd:\YourFreeStyleFolder\Data\Databases\content.db` into the `.ini` by hand.

### `content.db`

A plain, unencrypted SQLite database. The app opens it **read-only** and reads one table,
`ContentItems`:

| column | used for |
|---|---|
| `ContentItemId` | primary key; also names the item's art folder |
| `ContentItemTitleId` | stored in decimal; the identity used by collections and details |
| `ContentItemName`, `ContentItemGenre`, `ContentItemDeveloper`, `ContentItemPublisher`, `ContentItemRating`, `ContentItemReleaseDate` | display and the details panel fallback |
| `ContentItemPath` | install path **without the volume** (`\GAMES\Foo\default.xex`); unique per item |
| `ContentItemFileType` | 1 = loose XEX, 2 = loose XBE (original Xbox), 3 = STFS/GOD container |
| `ContentItemDiscsInSet` | multi-disc sets |
| `ContentItemContentType` | `XCONTENTTYPE_*`: `0x7000` 360 game, `0xD0000` arcade/XBLA, `0x5000` original Xbox, `0x2` indie |
| `ContentItemTab` | the FreeStyle category the item is filed under |

`ContentItemTab` uses FreeStyle's fixed numbering (its `Tabs` table in `settings.db`): 0 Automatic,
1 XBLA, 2 Xbox 360, 3 Xbox Classic, 4 Homebrew, 5 Emulator, 6 Other. These are the category tabs
the app shows. Only 0 ("Automatic") is resolved by content type (`0x7000` to Xbox 360, `0xD0000`
and `0x2` to XBLA, `0x5000` to Xbox Classic, anything else to Other). Out-of-range values become
Other. If the column is missing, every item falls back to the content-type rule rather than the
whole query failing. Content type alone is a poor category signal (homebrew often reports
`0x7000`), which is why the stored tab wins.

The app's SQLite VFS (`app/src/vfs_xbox.c`) has no file locking, and FreeStyle may rewrite the
database during a scan. So the app runs `pragma quick_check` before reading, and discards the whole
result if a step fails midway. In both cases it shows "Is FreeStyle scanning?" instead of a partial
library.

No other table is read. Launching from CollectionUI does not update FreeStyle's recently played
list.

### Homebrew: synthetic ids

A XEX without an execution-info header reaches the database with TitleId 0, which would merge all
homebrew into one item and keep them out of collections. Such items get a **synthetic id**: 32-bit
FNV-1a (offset 2166136261, prime 16777619) over `ContentItemPath` with ASCII letters lowercased;
a result of 0 becomes 1. It is stable across rescans and reboots. Moving or renaming the folder
changes it, and the item drops out of its collections.

### Cover art: `GameData` and the FSDA container

FreeStyle 3 keeps art outside the database, one folder per item:
`<install>\Data\GameData\<ContentItemId as 8 hex digits>\<same>.assets` (for example
`000000C4\000000C4.assets`). The `.assets` file is a big-endian container:

```
+0   char[4]  "FSDA"
+4   uint32   version (1)
+8   uint32   0
+12  uint32   bit mask of the types present
+16  uint32   entry count
+20  uint32   2
+24  entries, 16 bytes each: type, offset, size, size (repeated)
```

Each offset points to a DDS image inside the same file:

| type | content | usual size |
|---|---|---|
| 1 | icon | 64x64 |
| 2 | background | up to 1920x1080 |
| 4 | banner | 420x96 |
| 8 | small cover (front only) | 220x300 |
| 64 | unidentified | 420x320 |
| 128 | **full case wrap**: back, spine and front | 900x600 |

Details that matter when reading it:

- The table's size field is the **uncompressed** size (width x height x 4 + 128), not what is
  stored. The app computes the real DXT size from the DDS header and reads only that.
- The embedded DDS headers are **little-endian**, unlike the container around them.
- The images are DXT5 (sometimes DXT1) and **linear**, like any PC DDS: they decode directly, with
  no swizzle. On the console the app creates them as `D3DFMT_LIN_DXT5`/`LIN_DXT1`, so no tiling
  conversion happens.
- Type 128 is the whole case wrap. **The front is the right-hand 46.8%**, a fraction calibrated by
  hand against a real library. The app does not crop: it samples only that part of the texture
  (`FRENTE_U0 = 1 - 0.468` in `app/main.cpp`). The visible front is about 421x600, so cover slots
  use a 1.425 aspect ratio.
- Type 128 is not always there. Without it the app uses type 8, drawn whole. With neither, the item
  has no cover.

The other files in the folder (`GameCoverInfo.bin`, `GameAssetInfo.bin`, ...) are not used.

### Emulator ROMs

FreeStyle indexes only what the 360 can launch, so emulators show up as items but their ROMs do
not. The app scans for ROMs itself (`app/src/emuladores.cpp`):

- For every loose XEX (file type 1) in the library, it looks for a `Roms` or `games` folder next to
  the executable. Names are case-insensitive on the console, so these are the only two spellings
  checked.
- It scans that folder and **one level** of subfolders (FBANext keeps one subfolder per system),
  and accepts files by extension: `.smc .sfc .fig .bin .img .iso .cue .pbp .zip .nes .gba .gbc .gb
  .smd .gen .32x .pce .n64 .z64 .v64`. A `.bin`/`.img` is skipped when a `.cue`, `.pbp` or `.iso` of
  the same base name exists, so a PS1 rip shows up once.
- Each ROM becomes an item with the **emulator's** path and file type, always under the Emulator
  tab. Opening it launches the emulator, which starts at its own menu. None of the supported
  emulators accept a ROM at launch.
- **Synthetic id:** 32-bit FNV-1a seeded with `2166136261 XOR <emulator TitleId>`, over the ROM's
  path relative to the ROM folder (`Arcade\smw.smc`, or just `smw.smc` at the root), lowercased
  (ASCII and Latin-1). Renaming the file changes the id.
- Display names come from `capas\nomes.txt` next to the `.xex`, one `romfile|Display name` per
  line, `#` for comments (useful for MAME short names). The ROM file on disk is never renamed.
- Covers are loose images, `capas\<ROM name without extension>.jpg` or `.png` next to the `.xex`,
  produced on the PC by `ferramentas/capas-de-rom.py`.

## Files next to the `.xex`

| file | read/written | purpose |
|---|---|---|
| `colecoes.txt` | read at startup, rewritten on every change | the user's collections |
| `fichas.txt` | read at startup, optional | data for the details panel |
| `vault.txt` | written on demand | inventory export for xbox-vault |
| `emus.txt` | read/written at startup | cache of where ROM folders are |
| `collectionui.ini` | read; `banco=` written once | settings |
| `collectionui.log` | written | diagnostic log |
| `roteiro.txt` / `roteiro.lido` | read once, then renamed | control script for automated screenshots |
| `fotos\NAME.bmp` | written by the script | screenshots |
| `capas\`, `capas\nomes.txt` | read | ROM covers and display names |
| `media\` | read, required | `Arial_16.xpr`, `effects\SimpleShaders.fxobj`, `sounds\*.xma` |

### `colecoes.txt`

One collection per line, fields separated by `|`:

```
id|type|name|content
3|jogos|Couch co-op|4156081C,4D530910,555308B6
7|uniao|Co-op 2-4 players|3,4,5
9|intersecao|Unfinished co-op|7,8
```

- `id` is a decimal number, stable, used to reference the collection.
- The type is one of three words:
  - `jogos` ("games"): a hand-picked list. The content is TitleIds (real or synthetic) as 8 hex
    digits.
  - `uniao` ("union"): everything that is in **any** of the source collections.
  - `intersecao` ("intersection"): only what is in **all** of them.

  For `uniao` and `intersecao` the content is the source collection ids, in decimal.
- Sources follow a strict ladder, `jogos < uniao < intersecao`: a union takes only `jogos`
  collections, an intersection takes `jogos` or `uniao`. No type can contain its own type, so cycles
  are impossible and resolving needs at most two steps.
- A line with no `|` is a comment. `#` is **not** a comment marker, so a collection can be named
  "#1 favorites".
- **Lines with an unknown type are kept verbatim** and written back unchanged, so a newer version's
  collection type survives being opened by an older build. A composite line cut short (power loss
  mid-write) is also kept as is.
- Names are cut to 28 bytes without breaking a UTF-8 sequence, and `|` and line breaks become
  spaces.
- The old two-field format, `name|TitleIds`, is still read. Those lines get an id and are rewritten
  in the new format on the next save.
- Collections store TitleIds, so items that share one act as one: both discs of a multi-disc game,
  or two installs of the same game, are added and removed together.
- When a source collection is deleted, unions drop it and an empty union is deleted. Intersections
  are not pruned, since removing a factor would widen the result. They show as empty until fixed.

### `fichas.txt`

Data for the details panel (Y on a game). One line per game, UTF-8, TAB-separated: the TitleId in
hex, then `key=value` fields. `#` starts a comment, unknown keys are ignored, and lines over 4 KB
are skipped.

| key | value |
|---|---|
| `coop` | local co-op player count (from Co-Optimus) |
| `cx` | couch co-op extras, joined by ` · ` |
| `ct` | Co-Optimus text |
| `mc` | Metacritic score |
| `nj` | user rating, 0-5 (only when there is no Metacritic score) |
| `hm`, `hp`, `hc` | HowLongToBeat hours: main story, main + extras, completionist |
| `ano` | release year |
| `gen`, `dev`, `pub` | genre, developer, publisher |
| `desc` | description |

Text has to stay within Latin-1: the font atlas has no glyphs beyond it, and typographic quotes and
dashes become ASCII. The file is generated on the PC by `ferramentas/fichas.py`. Without it, the
panel shows only what FreeStyle stores.

### `vault.txt`

Written when the user picks Export (Back on the collections screen). It lists everything the
console has, so [xbox-vault](https://github.com/Lucas-Tito/xbox-vault) can import it. The export is
refused if the database could not be read or the library is empty, so an unread library never
looks like an empty one. Numeric fields are 8 hex digits unless noted:

```
JOGO|titleId|contentType||contentItemId|name|install path
APP|syntheticId|contentType||contentItemId|name|install path
ROM|syntheticId|contentType|emulatorTitleId||name|ROM file
COLECAO|id|type|name|content
```

- `JOGO` ("game") is an item with a real TitleId. `APP` is homebrew with a synthetic id, which
  will not match any catalog. `ROM` carries the emulator's TitleId and the relative ROM file name,
  the two inputs of its synthetic id.
- `COLECAO` ("collection") is a `colecoes.txt` line with the prefix added (decimal id and source
  ids, as in that file). Collections come after all items.
- Lines starting with `#` are comments. The last line is `# total: N jogos, N apps, N ROMs,
  N colecoes`, so a truncated file can be detected. There is no timestamp, because the console's
  clock is unreliable without a network.

### `emus.txt`

Scanning every loose XEX for a ROM folder is slow on a large library, so the result is cached. The
first non-comment line is a hex stamp (FNV-1a over the paths of all loose XEX items), followed by
`emulatorPath|romFolder` lines. If the stamp no longer matches, an emulator vanished or a folder is
missing, the full search runs again. ROMs themselves are always listed live. Delete the file to
force a new search.

### `collectionui.ini`

`key=value` lines: `banco=` (database path, see above), `som=0` (no sound), `anel=0` (no highlight
ring on marked games), `logDetalhe=1` (verbose log). Missing keys use the defaults.

### `collectionui.log`

Truncated on every run. Written with an immediate flush, so the last line survives a crash. Before
a launch the log is closed, and it is reopened in append mode if the launch fails. Fetch it over
FTP.

### `roteiro.txt`

A control script that presses buttons in place of the player, takes screenshots and returns to the
dashboard. When the app starts and finds it, it renames it to `roteiro.lido` ("read"), so a
forgotten script never takes over a normal session. One step per line, `#` for comments:

| step | effect |
|---|---|
| `espera MS` | wait MS milliseconds |
| `espera capas [MS]` | wait until no cover is loading (at most MS, default 8000) |
| `aperta BUTTON [N]` | press and release N times: `A B X Y START BACK LB RB CIMA BAIXO ESQUERDA DIREITA` (up, down, left, right) |
| `foto NAME` | save the frame as `fotos\NAME.bmp` (1280x720, 24-bit) |
| `teclado TEXT` | the next keyboard prompt returns TEXT without opening the Guide |
| `sai` | return to the dashboard (every script should end with this) |

An unknown step logs `roteiro: FALHA` and also exits to the dashboard, so the watcher never hangs.
The full spec is in `app/src/roteiro.h`, and an example is in
`ferramentas/roteiros/todas-as-telas.txt`.

## Launching

The database path has no volume, so the app tries each mounted alias (`Hdd:`, `Usb0:`...) until the
file exists. The launch call runs on a separate thread, because the XDK forbids it on the thread
that owns the D3D device. Pending I/O and the log are closed first. On success the console reboots
into the game. Code after the call only runs when the launch failed.

| item | how |
|---|---|
| Loose XEX / XBE (file type 1, 2) | `XLaunchNewImage(path, 0)` |
| Container, STFS/GOD (file type 3): retail, XBLA, arcade | `XContentLaunchImageFromFile(path, "default.xex")`, or `"default.xbe"` for content type `0x5000`. On a not-found error it retries with the `\??\`-prefixed path. |
| Emulator ROM | launches the emulator item; the ROM is picked in the emulator's menu |
| Indie (Xbox Live Indie Games) | see below |

**Indie games** have no XEX: the package (under `\Content\0000000000000000\584E07D2\00000002\`)
holds XNA assemblies and is opened by the system's `Xna_TitleLauncher.xex` on the extended system
partition (`\Device\Harddisk0\SystemExtPartition`, looked up in its version folder). The app sets
launch data with `XSetLaunchData`, 0x34 bytes: `0xCAFEBABE`, the package's file name and the storage
device id. Then it launches the XNA launcher with `XLaunchNewImage`. This is the same format
Dash Launch uses. **Implemented but not yet tested on hardware.**

## Controls

| screen | A | B | X | Y | Start | Back | LB / RB |
|---|---|---|---|---|---|---|---|
| Collections | open | | new collection | | options (rename, choose sources, delete) | export | switch tab |
| Unsorted tab (games in no collection) | play | back to All | add to collections | details | | | switch tab |
| Games in a collection | play | back | add games | details | options (remove from collection) | | jump by letter |
| Picking games / collections | select | cancel | | | done | | |

The tabs are "Unsorted" (shown only while some game is in no collection), "All", and each FreeStyle
category that has items. Holding down on a game grid also jumps by letter. The D-pad and left stick
navigate, with key repeat, and all four controllers are read. Lists sort alphabetically, ignoring a
leading article (`the a an o os as um uma`).

## Toolchain

The app is built with Microsoft's Xbox 360 XDK (2.0.21256) and its VS2010-era `cl.exe`, running
under wine in a Docker image derived from
[xdk-docker](https://github.com/ClementDreptin/xdk-docker). No Windows VM is needed, but the XDK is
proprietary and not in this repository. Rendering uses Direct3D 9 with the XDK's ATG sample
framework (font only) and the app's own drawing code. SQLite is vendored. The free alternative,
libxenon, was ruled out: its binaries run bare-metal from XeLL, with no Xbox kernel and no
`xam.xex`, so there is nothing to launch a retail game with. Setup and the pitfalls are in
[`toolchain/README.md`](../toolchain/README.md).

## Developer tools

- **`previa/`**: a PC preview of the console screen. A 1280x720 canvas whose drawing code mirrors
  the C++ `Desenhar()` call by call, with the same constants and the real font atlas.
  `previa/extrair.py` reads a copy of `content.db` and the `.assets` files and crops covers with
  Pillow. `previa/montar.py` builds a self-contained `previa/build/previa.html`. See
  `previa/README.md`.
- **`ferramentas/fotos-console.py`**: automated screenshots on the real console. It checks over
  xbdm that the console is idle on the dashboard, then uploads the `.xex`, `fichas.txt` and a
  control script over FTP. It starts the app, waits for it to return to the dashboard, downloads
  the log and the screenshots (converted to PNG), and checks that `colecoes.txt` did not change.
- **`ferramentas/fichas.py`**: generates `fichas.txt` from the console's `vault.txt` (`JOGO`
  lines), the preview's `games.json` and the xbox-vault data (Metacritic, HowLongToBeat,
  Co-Optimus).
- **`ferramentas/capas-de-rom.py`**: builds ROM covers at the grid slot size (146x208), with the
  whole box art over a blurred copy of itself, from a hand-written ROM-to-game table. Upload the
  output to `capas\` next to the `.xex`.
