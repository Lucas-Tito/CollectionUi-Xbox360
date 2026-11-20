# Preview

A PC reproduction of the console screen, for checking a change before copying it to the Xbox. The
page draws into a 1280×720 `canvas`, and its drawing code mirrors the C++ `Desenhar()` call by call:
same geometry, same colors, same text.

## Building it

```bash
python3 previa/extrair.py        # once: reads FreeStyle's content.db and crops the covers
python3 previa/montar.py         # writes previa/build/previa.html
```

`extrair.py` reads a copy of the console's FreeStyle data (`content.db` and `GameData/`, set with
`--dados`) and writes `previa/build/games.json` plus the cropped covers in `previa/build/img/`.
`--vault DIR` adds Metacritic and play-time fields from the
[xbox-vault](https://github.com/Lucas-Tito/xbox-vault) `data/*.json` files.

`montar.py` builds a single self-contained HTML page with the covers embedded. You can open it by
double-clicking. Options:

| option | what it does |
|---|---|
| `--vault vault.txt` | the inventory the app exports; adds homebrew and emulator ROMs, which `content.db` doesn't have, so the Homebrew and Emulators tabs appear |
| `--colecoes colecoes.txt` | the console's collections file; it takes precedence over what the browser saved |
| `--mock` | with `--colecoes`, puts a placeholder item in place of every TitleId the local library lacks, so the tabs and counts match the console |
| `--proposta file.js` | loads a script **on top of** the preview, to try a design change without touching the mirror |
| `--saida path.html` | output path |

## How it stays faithful

- **Constants are copies of `app/main.cpp`.** Geometry, colors and font scales. When they change
  there, they change here.
- **Text comes from the app's own font atlas.** At build time `previa/atlas.py` reads
  `app/media/Arial_16.xpr`, the file the app loads, and the page redraws each glyph the way the ATG
  font shader does. Widths, truncation and missing glyphs therefore match the TV. A character the
  atlas lacks is drawn as the "unknown" box and listed in red in the page's top bar.
- **`metro.js` is the current design** (decision 149 in [`../docs/design-decisions.md`](../docs/design-decisions.md)).
  `montar.py` always loads it over `template.html`, which keeps the shared base: atlas text,
  lists and navigation.
- **Details panel data** comes from `previa/ficha.py`. It reads the xbox-vault by TitleId and falls
  back to what FreeStyle stores. On the console, `ferramentas/fichas.py` writes the same data to
  `fichas.txt`.

The preview reproduces what the app draws, not what the TV does with it afterwards (gamma, black
level).

Known stand-ins:

- The game's category comes from its folder path, because `games.json` has no `ContentItemTab`
  column. On this library it gives the same result.
- The keyboard is an HTML `<input>`. On the console it is the system keyboard, and system
  notifications (`XNotifyQueueUI`) show in the page's top bar.

## Keys

| key | pad | action |
|---|---|---|
| arrows | d-pad | move |
| `Enter` | A | open a collection, play, or mark |
| `Esc` / `B` | B | back |
| `X` | X | new collection; add games or add to collections |
| `Y` | Y | game details |
| `M` | Start | options menu |
| `Backspace` | Back | export to Xbox Vault |
| `Q` / `E` | LB / RB | change tab on the collections screen; jump by letter inside a collection |
| hold `↓` | hold ↓ | jump by letter |

## Page vs. console

The canvas is the console screen. Anything drawn inside it is something the console would draw.
Development tools live in the page around it: the `?` button opens the button table, and two
more buttons create a set of sample collections and clear it.

Collections are kept in the browser's `localStorage`.
