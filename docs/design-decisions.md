# Design decisions

The condensed record of the decisions that still hold for the current app. It was distilled from a
round-by-round working log in Portuguese, which is still in the git history and also holds the dead
ends and superseded designs. Decision numbers come from that log, so code comments such as
"decisao 115" resolve here; gaps are decisions that were superseded or were only narrative. A
superseded decision that code still cites stays, with a note naming what replaced it.

Counts quoted below were measured on the development console. They are samples, not invariants
(D133).

## Product and environment

### D1 — A native app, not a dashboard
CollectionUI is its own `.xex`, opened like a game. It does not replace the boot dashboard and is
not a skin, script or DLL injected into FreeStyle or Aurora. It shows the installed library,
organized into user collections.

### D6 — Target: RGH/JTAG consoles running FreeStyle Dash 3
FreeStyle launches any `.xex` from its application list, so the app does not need to appear in the
stock dashboard's Games section.

### D15 / D17 — FreeStyle is a reference, not a base
The only open-source 360 dashboard that launches retail games is FreeStyle Dash 2 (one source dump
from July 2011; FSD 3 and Aurora were never opened; Xemini, Xenu and XMENU are libxenon and cannot
launch retail games). It is read to learn which APIs to call, never forked: its UI is XUI, it is
2.8 MB of code, and it is a dashboard. It is GPLv3, so copying an implementation would make this
project a derivative; the launch path (D35) uses XDK APIs only.

### D34 — Launching a game ends the app
Returning to the app after a game is not part of the experience, so there is no state to preserve.

### D84 — Superseded: no exit button
B on the collections screen was meant to return to the dashboard; it never worked and the owner
preferred not to have it. You leave by launching a game or through the Guide.
`lancador::Dashboard()` remains, used only by the control script (see Testing tools).

## Data sources

### D8 — FreeStyle's database, read-only
Measured on FSD 3.0.775:
- The library is `Data/Databases/content.db` in the FreeStyle folder (not `fsd2data.db`, as the
  FSD 2 source suggested): plain, unencrypted SQLite. The FSD 2 schema was still accurate; FSD 3
  only added `ContentItemHash` and `ContentItemKinectFlag`.
- `game:` is relative to the running title, so FreeStyle's `Game:\...` points into *our* folder.
  The app lists FreeStyle installs on the mounted devices and the user picks one once (`banco=` in
  `game:\collectionui.ini`).
- Art is not in the database. Each item has `Data/GameData/<ContentItemId hex>/` with an `FSDA`
  container of DDS images already in DXT5, so there is no image pipeline. Layout and types are in
  `app/src/fsda.h`. Type 128 is the full box art (back, spine, front; typically 900×600) and the
  front is its right 46.8% (`FRENTE_U0`); type 8 is a 220×300 small cover.
- The console clock reads 2005-11-22 when unset. Never trust it for dates.

### D10 — xbox-vault is optional enrichment
The [xbox-vault](https://github.com/Lucas-Tito/xbox-vault) catalog adds what FreeStyle lacks
(Metacritic, HowLongToBeat, co-op). The join key is the title id (hex string in the vault, INTEGER
in FreeStyle).

### D131 / D132 — Reading while FreeStyle rescans
The SQLite VFS (`app/src/vfs_xbox.c`) has no-op locks, so a FreeStyle scan can rewrite pages under
us; a corrupt page once crashed us writing to `0x41560914`, which was a TitleId. Defenses:
`pragma quick_check` first (catches settled corruption only); strings built from
`sqlite3_column_bytes` with a 1 KB cap, never by NUL; and any `step` that does not end in
`SQLITE_DONE` discards everything and fails, because half a library is worse than none. The app
then shows a system notification asking to wait for the scan.

### D133 — The large cover is not always there
On a 418-game library, 73 items had no type 128 (asset mask `0x2F` instead of `0xEF`). The loader
falls back to the small cover. A count from one library is a sample; note what the sample was.

### D134 — Whole cover vs. front crop belongs to the image
The same game yields box art (crop) or a small cover (whole) depending on its `.assets`, and ROM
covers are whole. The loader reports which it read and the flag travels with the texture in the
cache. This replaced a per-game flag (D130).

### Category source (part of D144)
Each item's FreeStyle tab is `ContentItems.ContentItemTab` (FreeStyle's fixed table: 0 Automatic,
1 XBLA, 2 Xbox 360, 3 Xbox Classic, 4 Homebrew, 5 Emulator, 6 Other): one more column in the
existing `SELECT`, not a join with `settings.db` and not a folder name. Only value 0 falls back to
`ContentType`, which alone is wrong (homebrew has `0x7000`, like retail). Indies land in XBLA.
Out-of-range values become Other. The query is retried without the column, because a missing
column fails `prepare` and would lose the whole library.

## Collections model

### D42 / D43 / D47 — Collections store 32-bit TitleIds
`ContentItemId` is keyed by path and dies on reinstall, move, rename or database rebuild; the
TitleId comes from the XEX header. Accepted cost: items sharing a TitleId (disc 1 and 2, duplicate
installs) merge, so marking one marks both. A hybrid `contentItemId:titleId` key was rejected; do
not propose it again. TitleIds use all 32 bits (Snes360 is `0xFFED0707`): parse with `strtoul`
base 16 into `unsigned int` and validate `!= 0` (with `atoi` and `> 0` they were silently dropped).
Zero is refused on write too. Homebrew without a TitleId gets a synthetic one, FNV-1a of the
lowercased install path (`biblioteca::IdDoApp`).

### D44 / D48 — Art by ContentItemId, counts by item
Art is still found by `ContentItemId`. Counts and labels count displayed items, not TitleIds.

### D45 / D46 — `colecoes.txt` parsing rules
Lines are read at any length: a 600-game line is ~5.4 KB, and a fixed buffer cut it; since every
save rewrites the file from what was read, the loss became permanent. The bar `|` separates data
from comments, not `#` (a collection named "#1 favorites" vanished); header lines have no bar.

### D104 / D143 — Three collection types
**Manual** (`jogos`, a TitleId list), **union** (`uniao`, in any source) and **intersection**
(`intersecao`, in all sources). Sources are referenced by collection id. Allowed sources follow a
strict ladder, `manual < union < intersection`: a union takes manual collections, an intersection
takes manual or union. Cycles are impossible by construction, `Tem()` recurses at most two levels,
and `PodeSerOrigem` is the single place the rule lives (picker and recursion guard). Example:
"co-op unfinished" = intersection of "unfinished" with the union of the co-op collections.
Composites hold no games; you remove a game from its source. Cards show a lightning emblem (union)
or two lenses (intersection), both procedural textures. `bool uniao` became `enum Tipo` so the
compiler listed all 19 uses; each `!uniao` would otherwise have silently changed meaning.

### D105 — Composites never store a resolved list
`Tem()` answers "is this game in?" for all types. A resolved vector would allocate in the draw loop.

### D106 / D143 — Dead sources
A union drops dead sources and is deleted when none remain, immediately on delete. An intersection
is never pruned: removing a factor *widens* it and the name starts lying. It keeps the dead source,
resolves to empty and shows `( 0 )` until fixed. "For all sources" over an empty list is true, so
`Tem()` requires at least one live source.

### D107 / D143 — Lines the reader does not understand are kept
Format: `id|type|name|content` (manual: hex TitleIds; composite: decimal collection ids). The
new-format test is the second bar, which the old `name|ids` format never has (a digits test broke
collections named `1942`). Unknown types are kept verbatim with their id reserved; a truncated
composite line is kept whole. Releases before v1.4 destroy intersection lines.

### D138 / D139 — Random collection ids
The file has two editors (this app and xbox-vault). `max + 1` reused freed ids and collided when
both sides created collections, silently retargeting unions. New ids are random 31-bit
(xorshift32); the fallback is the smallest free id. Duplicates found on load are renumbered for the
session (the second one) and each union pointing at them is logged, since the intent is
undecidable. A NUL-filled tail (power cut during save) used to crash every boot; it is handled.

### D100 — Deleting a collection asks first
The only destructive action. The confirmation opens with Cancel focused.

### D146 — "Unsorted" and archiving
The leftmost tab lists games in no manual collection (213 of 439 items on the development console).
Composites do not count. Covered TitleIds are sorted once per frame and binary-searched. It is a
screen, not a filter: cover grid, B returns to All, LB/RB stay on the tab bar. X opens the archive
screen (manual collections only; A marks, Start confirms). An archived game leaves the list at once
and the tab disappears when empty.

## Launching

### D35 / D57 — Two XDK calls
Loose XEX/XBE: `XLaunchNewImage` (`xbox.h:417`). Container (STFS/GOD):
`XContentLaunchImageFromFile` (`xbox.h:1293`), which mounts and launches in one call and returns an
error code. On not-found it is retried with the `\??\` prefix: our XAPI adds it for `Hdd:`, but this
call resolves in xam's context. Containers were confirmed in v1.1.0.

### D36 / D85 — `XLaunchNewImage` really is NORETURN
`cl.exe` rejects code after it (C4702) and Release deletes it. All diagnostics, including the
file-exists check, come before the call. Same for `XLaunchNewImage(NULL, 0)`.

### D37 — The game's device is found by probing
Paths in `content.db` have no volume, and FreeStyle's alias (`Hdd1:`) is not ours (`Hdd:`).
`lancador::Resolver` tries the mounted aliases with `GetFileAttributes`, `Hdd:` first. The database
can be on USB while the games are on the HDD.

### D38 / D39 / D40 — File types and what is left out
`ContentItemFileType`: 1 loose XEX, 2 loose XBE (original Xbox, launched like 1), 3 container. No
content-type whitelist (FSD 2's would reject indies); content type only picks `default.xex` or
`default.xbe`. No multi-disc swap (`PrepareForMultiDiscLaunch` needs FreeStyle's plugin) and no TU
consolidation (it deletes files).

### D41 / D50 / D64 / D66 — Tear down before launching
The XDK forbids launching with disk I/O pending or from the D3D device's thread. Stop the loader
(wait `INFINITE`), stop audio, release textures after `BlockUntilIdle`, close the log, launch from a
new thread. On failure, reopen the log in append mode and restart both loader and sound.

### D147 — Indie games launch through the system XNA launcher
An indie package has no XEX (just `GameInfo.xml` and XNA assemblies, title `584E07D2`, type
`00000002`). It is opened by `Xna_TitleLauncher.xex` in `\Device\Harddisk0\SystemExtPartition\
<version>\` (found by listing `xsep:`, `32000100` first). `XSetLaunchData` gets 0x34 bytes (magic
`0xCAFEBABE`, the package hash name, the `XCONTENTDEVICEID`), then `XLaunchNewImage(launcher, 0)`;
flag 0 keeps the data. Same as Dash Launch's `LaunchItem` and xkelib's `XLAUNCHDATA_CAFEBABE`. Our
XDK (2.0.21256) lacks `XContentCreateDeviceEnumerator`, so the device id comes from probing
`XContentGetDeviceData` for ids 1–32. Dash Launch's `fakelive`/`autofake` and `autocont` settings
matter. Not yet confirmed on hardware at the time of writing.

## Emulators and ROMs

### D108 — ROMs are not in any database
FreeStyle indexes only what the 360 launches. Emulators are ordinary items; ROMs are loose files in
folders next to them, and CollectionUI scans those folders.

### D109 — Synthetic ids for ROMs
All ROMs of an emulator share its TitleId. A ROM's id is FNV-1a 32 of (emulator TitleId, lowercased
file name with its subfolder, D124), stored in the TitleId field, so `Tem`, `Alternar`, `Remover`
and the file format are unchanged. A collision with a real TitleId behaves like a multi-disc pair.
Lowercasing covers Latin-1 (D121); `char` is signed in the XDK's `cl.exe`, so `c >= 0xC0` without an
unsigned cast never matches.

### D113 — ROMs open the emulator's menu
`XSetLaunchData` exists in `xav.lib` without a header, but Snes360 and FBANext read neither launch
data, nor the command line, nor an autoload setting (checked in their sources). The scene has two
incompatible conventions: Aurora's `'AUOA'`/`'ROMS'` 656-byte struct (pcsxr-360 2.1.1a, RetroArch up
to 1.7.0) and modern RetroArch's raw NUL-terminated path. Launch data is per target; for the current
emulators there is nothing to send.

### D117 — ROMs reuse the game launcher
A ROM item carries the emulator's path and file type, so opening it goes through the same launch
code. Its `id` is negative (`-(n+1)`) because `id` is the texture-cache key and `ContentItemId` is
always positive. The synthetic TitleId (D109) is what goes to disk. The scan has no per-emulator
table: `Roms` and `games` folders next to the emulator, a generous extension list, and the device
from `lancador::Resolver`.

### D119 / D120 / D124 — Scan rules
`FindFirstFile` is case-insensitive, so list one spelling per folder (`Roms`/`roms`/`ROMS` tripled
every ROM). `.cue`/`.pbp`/`.iso` cover the `.bin`/`.img` with the same base, so a disc is one item.
Scan one level of subfolders (FBANext keeps per-system folders); the subfolder name enters the id,
empty at the root so older ids did not change.

### D127 / D128 — Arcade zip names are romset ids
FBANext asks the driver which zip to open (`bzip.cpp:374-388`), so `mslug3.zip` cannot be renamed.
Display names come from `capas\nomes.txt` (`rom file|Display name`); ids and covers use the file
name, so relabeling changes nothing else.

### D141 — Cache the emulator roots, never the ROMs
The ROM scan was 86% of startup (6.2 of 7.2 s), mostly ~438 negative probes proving ordinary XEXs
are not emulators. `game:\emus.txt` caches each emulator's path and ROM folder, stamped with an
in-memory FNV-1a of all loose-XEX paths; a new set forces a full search, and each cached root gets
one existence check. ROMs are still listed live, so FTP uploads show up next boot and ids always
come from the current file name. Dropping `games` or parallelizing (one disk head) were rejected.

## ROM cover art

### D111 / D116 / D122 — ROM covers are plain image files
`D3DXCreateTextureFromFileInMemoryEx` decodes JPG, PNG, BMP, DDS and TGA. `Jogo::capa` holds the
cover path (`.assets` or an image), and the loader decides by content: `FSDA` magic means container,
anything else is read whole, capped at 8 MB (a wrong path could hit a 600 MB `.bin`). DDS size is
read from the header only after confirming it is DDS.

### D114 / D125 / D129 — Covers are composed on the PC at 146×208
`ferramentas/capas-de-rom.py` builds them from xbox-vault art (arcade from libretro's FBNeo set,
whose flyers are already portrait). The slot is portrait 1:1.425; US SNES boxes are landscape and
PS1 jewel cases square. Each cover is fit whole over a blurred copy of itself, and covers wider than
1.15 are rotated 90° counter-clockwise (99% of the card instead of 49%). The rule is by aspect, not
system: Japanese SNES boxes are portrait. The app has no letterboxing logic.

### D115 — The app finds a ROM cover by file name only
`<art folder>\<ROM name without extension>.jpg`, then `.png`, else none. No normalization, no fuzzy
matching. The title-to-art mapping is a hand-written table in the PC script: fuzzy matching got 13
of 16 ROMs right but one wrong *silently* ("Super Mario World 2" scored 0.94 against "Super Mario
World"; it is Yoshi's Island), and all four approximate arcade matches were wrong. A wrong cover
without warning is worse than a missing one. To fix a cover, drop an image named after the ROM.

## Export to xbox-vault

### D142 / D139 — One file, `game:\vault.txt`, on demand
Library and collections go out in one write, so the vault never pairs files from different moments.
`colecoes.txt` is the live file; the export is a snapshot never read back. Item lines
(`JOGO|APP|ROM`) come first, then `COLECAO|id|type|name|content` (the `colecoes.txt` line with a
prefix, so the vault reuses its parser). `item` is the `ContentItemId`, since TitleId is not unique;
ROM lines carry the emulator TitleId and file name that seed the synthetic id. The header documents
the format without a bar, and a footer with counts exposes truncation. No timestamp (the clock lies
silently). A failed library read returns an empty list, so the export takes `bancoOk` and refuses
rather than send an empty inventory that looks real. Write errors are checked at `fclose`.

### D145 — Export lives on Back, behind a confirmation
The export covers everything, so it left the per-collection menu for the Back button on the
collections screen (previously unused), with a confirmation because it overwrites `vault.txt`. Not
available while picking sources. Collection menus put the destructive item last, red only while
focused. The modal's original visual design was replaced by the Metro dialog (D149).

## UI and visual design

### D148 — All on-screen text is English
Tabs: Unsorted, All, XBLA, Xbox 360, Xbox OG, Homebrew, Emulators, Other. Types: Manual collection,
Union, Intersection. Footer: Play, Back, Select, Done, Options, Export. The log, the comment headers
of `colecoes.txt`/`vault.txt` and the code are unchanged; collection names are never translated.

### D149 — The Metro design
The look follows the Xbox 360 Metro dashboard screen by screen, ported from `previa/metro.js` with
the same numbers (`app/main.cpp`, block "O DESENHO METRO"); colors measured from direct captures:
- background gradient from (80,78,84) at the top to (177,176,180) at 78% height;
- tabs are lowercase Metro hubs on the left; title and counter at top right;
- collection tiles are the 244 px square inside a white card insert that dims when unselected; games
  are "My Games" cards;
- focus grows 8 px per side and lifts with a shadow; marked items keep a 2 px green outline;
- menus are the Metro "Delete Profile" dialog measured at 720p (548 px; focused row green, red if
  destructive);
- Y opens a details panel rising from the bottom with xbox-vault data from `game:\fichas.txt`
  (`ferramentas/fichas.py`), offered only for games with data; A still plays directly.

Animations were measured frame by frame from a capture of dashboard 2.0.14448. Focus is exponential
(τ = 40 ms). Tab change (both pages visible) and the details panel use a cubic ease-out of 660 and
210 ms; the measured exponentials (220, 70 ms) never arrive and were cut with a visible ~9 px jump.
Screen changes crossfade over 200 ms from the previous frame resolved into a texture. Text drops the
atlas's black shadow via a second pixel shader (`ATG::g_fonteSemSombra`), except on tile names.

### D144 — Category tabs on LB/RB
A tab narrows the universe being asked about; it never changes a collection's contents, so
`colecoes.cpp` knows nothing about tabs. A tab exists only if some collection has a game in it, and
collections with nothing in the tab are hidden, so no key leads to an empty screen; with one
category there is no bar. Tabs do not apply while picking sources, archiving, adding games or on
Unsorted (`AbaEmVigor()` decides, in one place). The tab lives only in memory: half the library
missing after a forgotten RB looks like a bug. Creating, editing, or marking a game outside the tab
returns to All. A tab change is the only action of its frame (LB+A once opened the wrong collection).
Screen indices go through `ListaDeColecoes()`, never `Ordenadas()` (delete once hit the wrong one).

### D20 / D22 / D23 / D24 / D76 / D77 — Grids
Collections start empty with no "+" tile, are alphabetical and never reordered; 244 px tiles, 4 per
row, centered, scrolling one row at a time. Games: 5 per row, alphabetical ignoring a leading
article, names cut by measured width. Letter jump by holding down, or LB/RB inside a collection, with
an A–Z/# index on the right that lights the current letter and dims absent ones.

### D21 / D25 / D27 / D32 / D33 — Buttons
A confirms, B backs out or cancels, X adds (new collection; add games to a manual collection;
nothing in a composite), Start opens options for the focused item, Back exports, Y shows details. A
new collection asks only for a name and starts empty. In "Add games", Start finishes and B truly
cancels: marks go to a copy. The footer is the only control documentation and shows a hint only when
the button does something.

### D79 / D112 — Small rules
All four controllers count (buttons ORed; the stick furthest from center wins). Uppercase only on
collection tiles, applied before measuring, without `towupper` (ASCII-only in the "C" locale):
Latin-1 0x00E0–0x00FE minus 0x20, skipping 0x00F7.

## Rendering and console quirks

### D49 / D55 / D56 — ATG DebugDraw pitfalls
`DrawScreenSpaceTexturedRectColored` forces UV 0..1 (no sub-rectangles) and with a `NULL` texture
samples an unbound sampler; use `DrawScreenSpaceRect(r, 0.0f, color)` for solid fills. DebugDraw
never enables `D3DRS_ALPHABLENDENABLE`, so alpha is ignored unless you turn it on (and off).

### D74 / D75 / font atlas — Text encoding
With a BOM, `cl.exe` converts narrow literals to the execution code page (`"ção"` → `e7 e3 ...`), so
`MultiByteToWideChar(CP_UTF8)` stopped at the first accent. `Larga()` validates UTF-8 itself
(`MB_ERR_INVALID_CHARS` does not exist here) and falls back to Latin-1. `Arial_16.xpr` stops at
U+20AC and renders anything beyond as a box, so no `∪ ∩ ✓`; `Larga()` maps curly quotes, dashes,
ellipsis, ™ and ® to atlas characters. FreeStyle stores U+FFFD where ® was. ATG's `DrawText` treats
a negative x as "from the right edge", so `Texto()` shifts the font window for text sliding off left.

### D78 — Covers are `D3DFMT_LIN_DXT5`
360 textures are tiled; DDS data is linear. Asking for `D3DFMT_DXT5` made D3DX convert every cover
(the only difference from FreeStyle's call). Size is passed explicitly from the DDS header. Check how
FreeStyle does it before writing, not after it breaks.

### D86 — `ATG::Font::DrawText` overruns 64 bytes when truncating
With `ATGFONT_TRUNCATED` it reserves `4 * (wcslen + 3)` vertices, but the third ellipsis dot breaks
out before `dwNumChars--` and the fill loop writes one extra quad past the reservation: intermittent
access violations on screens with truncated names. The fix (decrement before the `break`) is ours,
carried in `toolchain/patches/atg.patch`. It cost eight console rounds and five wrong hypotheses.

### D95 — No font, no app
Without it the first `DrawText` dereferences a null `m_TranslatorTable`; the app exits saying so.

### D51 / D52 / D53 / D54 — The system keyboard is asynchronous
Blocking on `XShowKeyboardUI` freezes the console: the Guide is composed over the title's frame, so a
title stuck in a wait leaves nothing to compose. Start it, keep drawing, poll
`XHasOverlappedIoCompleted` each frame. Buffers must not be on the stack. Use `XUSER_INDEX_ANY`
(there may be no profile). Buttons are ignored while it is open, so its confirming A never leaks.

### D71 — The loader thread runs on hardware thread 2
XAudio2 defaults to threads 4|5. The loader on thread 4 fought the audio engine and crashed only on
entering a collection, when `.assets` reading starts. Drawing is on 0.

## Sound

### D58 / D59 — FreeStyle's skin sounds through XAudio2
Five `.xma` effects from FreeStyle's default skin, versioned in `app/media/sounds`. They are XMA2
(`0x166`), decoded in hardware, so no PCM conversion. FreeStyle plays them via XUI scenes; we call
XAudio2 directly rather than adopt XUI.

### D60 / D61 / D68 / D69 / D70 / D80 — XMA buffer rules
XMA must be 2 KB aligned in physical memory: `XPhysicalAlloc` with alignment 0 (page size, 4096).
The RIFF header is little-endian: `LocalizeXma2Format()`, and chunk sizes read byte by byte
(unaligned reads can fault on PowerPC). The XMA payload is never byte-swapped. A `memcpy` into
physical memory needs no flush: the South Bridge snoops L2. Test chunk bounds by subtraction;
`p + 8 + size > total` wrapped and looped forever before the first frame.

### D63 / D65 / D67 / D72 / D101 — Playback behavior and levels
Sound never takes the app down; failures are logged and the app goes silent. `Tocar()` skips a sound
still playing (`BuffersQueued > 0`); Stop/Flush/Submit queues behind it, since Stop is asynchronous.
Sounds play only when something happened. The files peak at full scale and resampling to 48 kHz
clips, so a master `VOLUME` (now 0.32) applies, with per-effect gains leveling them near −19.5 dBFS.

### D91 — On a crash, stop the audio engine
With audio running, a crash hung the console. The exception filter calls `IXAudio2::StopEngine` (a
full teardown allocates and waits) and returns `EXCEPTION_CONTINUE_SEARCH`, because
`EXCEPTION_EXECUTE_HANDLER` "usually results in the game console freezing".

## Logging and diagnostics

### D30 / D82 / D83 — A log you can trust
`game:\collectionui.log`, next to the XEX, read over FTP or USB; no XBDM. Writes use `WriteFile` +
`FlushFileBuffers` under a lock: `fflush` stops at the file-system cache, which a crash loses, and
three rounds of diagnosis rested on a log whose end was not the end of the run (D89).

### D81 / D88 / D90 / D96 — Make the program say where it died
An unhandled-exception filter logs `ExceptionCode`, `Iar`, `Lr`, read/write and the faulting
address; the link emits a `.map`. ATG's silent `FatalError` is patched to write to our log. Locate a
crash before guessing: `.ini` switches (`som=0`, `anel=0`) yield one bit per run.

### D97 / D98 / D99 — Detail logging and the `.ini`
Per-texture detail lines are on only with `logDetalhe=1` in `collectionui.ini`, handled in `diario`
so every module obeys. Releases ship no `.ini`, so detail is off by construction. Rewriting the
`.ini` keeps keys it does not own; an empty value enables nothing.

### D135 — Startup is timed
The log records `[N ms]` per startup phase. Sorting moves pointers, and new lists are merged in
(`Juntar`) instead of appended and re-sorted (D118, D123).

## Build and toolchain

### D7 / D11 / D12 / D28 — XDK under wine in Docker
Only the official XDK builds a `.xex` that runs under the kernel with access to `xam`, which launches
games; libxenon cannot. `cl.exe`, `link.exe` and `imagexex.exe` run under wine in a container on
Linux. The traps are in [`toolchain/README.md`](../toolchain/README.md). OpenXeChain is the free
alternative to watch, not usable yet.

### D14 — Superseded: ATG instead of Dear ImGui
The UI was planned on imgui-xbox360; it is drawn with the XDK's ATG sample framework and our own D3D.

### D31 — Build Release for the console
The Debug `d3d9d.lib` expects a devkit with XBDM and fails confusingly on retail hardware.

### D62 — XDK libraries are copied into the image one by one
Sound needed `xaudio2.lib` (debug: `xaudiod2.lib`) and `xmcore.lib` (XAudio2's lock-free queue). A
new XDK library means editing the Dockerfile and rebuilding.

### D92 / D93 — What is versioned
ATG sources and XDK media (`Arial_16.xpr`, `SimpleShaders.fxobj`, whose absence crashes at startup)
are proprietary; `toolchain/vendor-atg.sh` copies them and applies `toolchain/patches/atg.patch`
(after normalizing CRLF), aborting if it fails. Small public files like the sounds are versioned.

### D140 — Every object depends on every header
The rule was `%.obj: %.cpp`, so header changes rebuilt nothing. Adding a field to `Jogo` linked two
`sizeof`s into one binary and crashed at startup reading `0xFFFFFFF8`. Our objects now depend on all
headers; vendor objects only on vendor headers. `-showIncludes` was not worth translating wine paths
for 14 sources. The rules must follow `all`, or the first `.obj` becomes the default target.

### D73 — Upload the XEX under a temporary name
FreeStyle's FTP has no `REST`. Upload under another name, check the size, `DELE` + `RNFR`/`RNTO`,
verify by md5. An aborted upload locks the temp file until the session drops.

### D102 / D103 — Versioning
`MAJOR.MINOR.PATCH` by what changes for the user. MAJOR: a data break (a `colecoes.txt` the previous
version cannot read) or a change of purpose. MINOR: the user can do something new, including
something that existed but did not work (containers were `1.1.0`). PATCH: fixes, UI, performance.
Release notes never claim an unverified test result.

## Testing tools

Not numbered decisions; listed so the workflow is discoverable.

- **Preview** (`previa/`): a PC page that mirrors `Desenhar()` call by call, with the same constants
  and the app's own font atlas, so widths and truncation match the TV. Designs are tried there first.
  See [`previa/README.md`](../previa/README.md).
- **Control script** (`app/src/roteiro.h`): `game:\roteiro.txt` presses buttons, waits for covers,
  saves screenshots to `game:\fotos\` and returns to the dashboard; it is renamed on start so a
  forgotten file never takes over a normal launch. `ferramentas/roteiros/` visits every screen
  without saving anything.
- **`ferramentas/fotos-console.py`**: uploads the build and a script over FTP (D73), launches the
  app through XBDM only when the console is idle on the dashboard, waits for it to return, downloads
  the log and screenshots, and checks that `colecoes.txt` did not change. The console address comes
  from local config outside the repo.
- **`ferramentas/fichas.py`** builds `fichas.txt` for the details panel from `vault.txt` and
  xbox-vault data; **`ferramentas/capas-de-rom.py`** builds ROM covers (D114, D115).
