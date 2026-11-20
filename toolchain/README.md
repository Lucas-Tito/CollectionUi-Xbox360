# Toolchain

Builds an Xbox 360 `.xex` on Linux, with no Windows VM and no Visual Studio install: the XDK's own
compiler and tools run under wine inside a Docker image. The XDK's `imagexex -dump` reads the
result as a title module with load address `82000000`.

## Prerequisites

- **Docker** (rootless is fine)
- **cabextract**
- The **Xbox 360 XDK 2.0.21256.x**. It is Microsoft's proprietary software, is not in this
  repository and never will be: `/sdk/` is git-ignored.

### The SDK used

To check that another copy is the same thing:

| | |
|---|---|
| file | `XBOX360 SDK 21256.3.exe` |
| size | 1,526,672,200 bytes |
| md5 | `b329b79db2804e2989688bffbaf49691` |
| format | InstallShield self-extracting PE, dated 2013-12-06 |
| extracted | 5.11 GB in 6,536 files |
| version the tools report | `2.0.21256.0` (from `imagexex -dump`) |

`cabextract` reads straight from the `.exe`, with no wine and no installer. That is why the setup
step is one command instead of a `wine setup.exe` session.

## One-time setup

```bash
mkdir -p sdk && cd sdk
cabextract -d . "XBOX360 SDK 21256.3.exe"     # 5.1 GB, ~15 s
cp ../toolchain/dockerignore .dockerignore
docker build -t collectionui-xdk:light --build-arg XDK=XDK -f ../toolchain/Dockerfile .
```

The first build compiles a minimal wine from source and takes a while. After that it is cached.

The app also uses the XDK samples' ATG framework, with a small patch. To copy it into
`app/vendor/atg` and apply the patch, run `toolchain/vendor-atg.sh`.

## Building

```bash
./toolchain/build.sh app                   # Debug   -> app/build/Debug/bin/collectionui.xex
./toolchain/build.sh app CONFIG=Release    # Release -> app/build/Release/bin/collectionui.xex
```

`build.sh` runs `make` inside the container and then gives file ownership back to your user.

## Three traps, and why each fix exists

None of these is documented in the upstream `xdk-docker`, and each one cost a debugging round.

**1. Upstream's wine pruning is out of date.** The upstream `light.Dockerfile` clones wine from
*master*, then runs `scripts/keep-needed-files.sh`, which keeps a **fixed list** of DLLs written
for an older wine. Current wine's `setupapi` depends on `cfgmgr32.dll`, which isn't on the list.
Without it `wineboot` can't create the prefix and the build dies before compiling anything, with
`unimplemented function setupapi.dll.SetupDiCreateDeviceInfoList`. **Our Dockerfile doesn't
prune.** The image grows from 677 MB to 1.56 GB, which is worth it.

**2. The C and C++ headers aren't where the sample Makefile looks.** In this SDK, `include/xbox`
holds only the Xbox headers (`xtl.h`, `d3d9.h`; 149 files). The standard ones (`excpt.h`, `stdio.h`,
`<string>`, `<vector>`; 166 files) live in `TechPreview/Jul12Compiler/include/xbox`. Without both,
`xtl.h` fails on its first line with `Cannot open include file: 'excpt.h'`. The Dockerfile copies
the second set to `/xdk/include/crt`, and the Makefile points `INCLUDE` at both.

**3. Don't bake `WINEPREFIX` into the image.** It's tempting to create the prefix once at image
build time, to save seconds per run and to run the container with `--user`. **It doesn't work:**
wine refuses a prefix not owned by the process owner (`'/wine-prefix' is not owned by you`), and a
prefix created that way lacks the user's `Temp` directory, which breaks `cl.exe` with
`D8037: cannot create temporary il file`. The fix is to let wine create the prefix on each run,
run as root, and fix ownership at the end, which is what `build.sh` does.

## Files

| | |
|---|---|
| `Dockerfile` | wine + XDK image. Based on upstream's `light`, without the pruning, with D3D/XGraphics/XJSON/XNet, `fxc.exe`, `xbcp.exe` and the CRT headers |
| `dockerignore` | copy to `sdk/.dockerignore`; drops `art`, `Source` and `doc` from the build context (6.6 GB → 2.9 GB) |
| `Makefile` | project template, with `INCLUDE` pointing at both header directories |
| `build.sh` | builds and fixes ownership |
| `vendor-atg.sh` | copies the ATG sample framework into `app/vendor/atg` and applies `patches/atg.patch` |
| `patches/atg.patch` | the app's changes to ATG (font shader without the drop shadow, among others) |
