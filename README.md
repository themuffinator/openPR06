<a id="top"></a>

<div align="center">

<img src="assets/docs/img/banner.png" alt="openPR06">

**The modern way to play PREY.**<br>
An open-source engine and game-code replacement that runs the original 2006 game on today's Windows, Linux and Mac computers.

[![Status](https://img.shields.io/badge/status-early%20development-d97a1f.svg)](docs/user/release-status.md)
[![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20Linux-lightgrey.svg)](#platforms-and-requirements)
[![Licensing](https://img.shields.io/badge/licence-GPL%20engine%20%7C%20SDK%20game%20code-blue.svg)](LICENSING.md)
[![Proudly vibecoded](assets/docs/img/badge-proudly-vibecoded.svg)](AGENTS.md)

[![Build openPR06 from source](https://img.shields.io/badge/Build-openPR06%20from%20source-2d8f4e?style=for-the-badge&logo=github)](BUILDING.md)
[![Join the DarkMatter Discord](https://img.shields.io/badge/Join%20the-Discord-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/T32mFejwR4)

[Quick start](#quick-start) · [Features](#features) · [Platforms](#platforms-and-requirements) · [Guides](#player-guides) · [Help](#getting-help) · [Build from source](BUILDING.md)

</div>

## What is openPR06?

openPR06 replaces PREY's engine and game binaries with its own: a GPL engine built on [openQ4](https://github.com/themuffinator/openQ4) and a source-available game module built from the PREY SDK. Your original campaign and multiplayer maps load from the retail game files, now with modern window and display handling, a choice of OpenGL or Vulkan rendering and one game module for single-player and multiplayer.

> [!NOTE]
> openPR06 does **not** include any PREY game data. You need your own copy of PREY (2006), from the original discs or another legitimate source.

> [!IMPORTANT]
> openPR06 is in **early development**. Its first release is [0.15.0, the First Contact Edition](https://github.com/themuffinator/openPR06/releases/tag/v0.15.0). Representative campaign maps, save/load and a two-client multiplayer session have been played on it, but full campaign completion and multiplayer parity aren't claimed. See [release status](docs/user/release-status.md).

## Quick start

1. Install **PREY (2006)**. Leave it where it is; openPR06 keeps to its own folder.
2. Download the [latest release](https://github.com/themuffinator/openPR06/releases/latest) for your platform: the Windows installer or zip (x64 or ARM64), the Linux `.tar.xz` or AppImage (x64, or the ARM64 preview), or the macOS `.app` (Apple silicon, preview). Install or extract it into a folder of its own.
3. Launch `openPR06-client_x64` (`.exe` on Windows; `_arm64` on ARM64), the AppImage, or `openPR06.app`.
4. openPR06 finds your PREY install on its own. If it doesn't, launch with `+set r_fullscreen 0 +set fs_basepath "C:\path\to\Prey"`, pointing at the folder that contains `base`, not at `base` itself. See [If auto-detection fails](docs/user/getting-started.md#if-auto-detection-fails).

> [!TIP]
> Don't copy retail `.pk4` files into `basepr/`. That folder is for openPR06's own files.

To build it yourself instead, follow [BUILDING.md](BUILDING.md); the staged package lands in `.install/`. The nightly builds from April 2026 further down the [Releases page](https://github.com/themuffinator/openPR06/releases) predate openPR06's move onto the openQ4 engine; use 0.15.0 or later.

## Features

What the current code does:

- 💿 **Finds CD-era installs.** openPR06 checks the folder it starts in, the Windows registry (install keys, App Paths and uninstall entries) and the usual install folders such as `Human Head Studios\Prey` and `2K Games\Prey`. It doesn't depend on Steam or GOG. On Linux it looks in `~/Games/Prey`.
- ✅ **Checks your game files.** Startup confirms the official base packs are present. The consolidated retail layout is checksum-verified; CD-era numbered packs are checked for presence until their checksums are recorded.
- 🎮 **One module for every mode.** A single `game_x64` module, built from the PREY SDK, runs single-player and multiplayer.
- 🖥️ **Modern window handling** through SDL3: windowed, borderless, desktop fullscreen and exclusive fullscreen, monitor selection with `r_screen`, and aspect ratio and FOV that follow the render size. Linux runs native Wayland or X11.
- 🎨 **OpenGL or Vulkan.** OpenGL is the default and matches PREY's original interaction stages. The Vulkan renderer (`r_renderApi vulkan`) reaches gameplay in focused tests but isn't at full parity yet.
- 💡 **Baked light grids** add indirect light to portal areas: bake them, inspect them and switch them on. See [Light Grids](docs/user/light-grids.md).
- 🤖 **Multiplayer bots** (experimental): a cast of bot characters on PREY's own artificial player, each with a fighting style and chat. Retail maps ship no bot navigation, so bots fight what they can see and don't plan routes. See [the bot guide](docs/dev/mp-bots.md).
- 🧪 **Inherited from openQ4, not yet checked against PREY**: shadow maps, cel shading, DDS texture replacements, controller support with a Steam Deck profile, performance presets with Auto-Detect, and the level-load cache. Their guides are [below](#player-guides).

The [rebase status ledger](docs/dev/prey-rebase/status-ledger.md) records what has been ported and how each piece was tested.

## Platforms and requirements

Active CI builds Windows x64 and Linux x64 on every push. Releases are built on openQ4's platform set: Windows x64 and ARM64, Linux x64 and ARM64 with AppImages, and macOS on Apple silicon in OpenGL and Metal-bridge variants. Only Windows x64 has had real PREY play-testing; the others are built, packaged and checked in CI but not yet played on. See [release status](docs/user/release-status.md).

| Platform | State | Notes |
|---|---|---|
| **Windows x64** | Released: installer and zip; most runtime testing happens here | Windows 10/11 recommended |
| **Windows ARM64** | Released: installer and zip | Native ARM64 build for Windows 11 on Arm; not yet play-tested with PREY |
| **Linux x64** | Released: `.tar.xz` and `x86_64` AppImage | A modern 64-bit desktop, comparable to Ubuntu 24.04 or newer. SDL3 picks Wayland or X11; set `OPENPR06_FORCE_X11=1` to force XWayland |
| **Linux ARM64** | Preview: `-preview` `.tar.xz` and `aarch64` AppImage | Built and checked on ARM64 runners; no real-hardware PREY play-testing yet |
| **Steam Deck / SteamOS** | Launcher in Linux builds | Run `openPR06-steamdeck`; inherited from openQ4, not yet checked with PREY. See the [Steam Deck guide](docs/user/steam-deck.md) |
| **macOS (Apple silicon)** | Preview: unsigned `openPR06.app`, OpenGL and Metal-bridge variants | macOS 11 or newer. Not signed or notarized, so Gatekeeper asks first; see [Getting Started](docs/user/getting-started.md#macos). Not yet play-tested with PREY |
| **Android** | Not supported yet | The Android host inherited from openQ4 isn't ported to PREY |

You'll need a 64-bit CPU, a graphics driver with OpenGL ARB2-era vertex and fragment program support, and PREY (2006)'s retail `base/` files. Details are in [Getting Started](docs/user/getting-started.md#system-requirements).

## Player guides

Start with [release status](docs/user/release-status.md): what works today and what isn't claimed yet. Most other guides describe engine features carried over from openQ4, and each one says where PREY testing is still pending.

**Set up**
- [Getting Started](docs/user/getting-started.md): install layout, platform notes and [what to do if auto-detection fails](docs/user/getting-started.md#if-auto-detection-fails)
- [Client Settings](docs/user/client-settings.md): where the most useful options live
- [Display Settings](docs/user/display-settings.md) · [Input Settings](docs/user/input-settings.md) · [Gameplay Settings](docs/user/gameplay-settings.md) · [Steam Deck](docs/user/steam-deck.md)
- [Virtual Reality](docs/user/vr.md) (not yet checked with PREY) · [macOS Support Data](docs/user/macos-support-data.md) (macOS isn't supported yet)

**Play**
- [Multiplayer Networking](docs/user/multiplayer-networking.md) (experimental) · [Multiplayer Chat](docs/user/multiplayer-chat.md)
- [Multi-View Demos](docs/user/multiview-demos.md): why MVD recording and playback are unavailable for now

**Look and feel**
- [Shadow Mapping](docs/user/shadow-mapping.md) · [Light Grids](docs/user/light-grids.md) · [Cel Shading](docs/user/cel-shading.md)
- [Temporal AA and Dynamic Resolution](docs/user/temporal-presentation.md) · [Advanced Screen-Space Lighting](docs/user/advanced-screen-space-lighting.md) (both experimental)
- [DDS Texture Replacements](docs/user/texture-replacements.md) · [PBR Materials](docs/user/pbr-materials.md) · [Level-Load Cache](docs/user/level-load-cache.md)

**Host and create**
- [Server Setup](docs/user/server-setup.md) · [Server Security](docs/user/server-security.md)
- [Map Entity Strings](docs/user/map-entity-strings.md): for mappers and modders

## Compatibility

- openPR06 runs the **official retail PREY (2006) assets** with its **own engine and game module**.
- Mods built for the original proprietary PREY game DLLs won't load.
- For multiplayer, use matching openPR06 builds on every client and server.
- openPR06 is in **early development**: compatibility work is ongoing.

## Getting help

- **Something broken?** Open an [issue](https://github.com/themuffinator/openPR06/issues) with your setup details and a log: launch with `+set logFile 2`, reproduce the problem and attach the `qconsole.log` it writes to your save folder.
- **Crashed on Windows?** Attach the log and minidump from the `crashes/` folder beside the executable.
- **Building on a Mac?** macOS isn't supported yet, but the [macOS support-data guide](docs/user/macos-support-data.md) shows what a useful report contains.
- **Want to chat?** Find us on the [DarkMatter Discord](https://discord.gg/T32mFejwR4).

## For developers

- [BUILDING.md](BUILDING.md): build openPR06 on Windows, Linux and macOS hosts
- [TECHNICAL.md](TECHNICAL.md): compatibility status, file layout, configuration and dependencies
- [AGENTS.md](AGENTS.md): project rules, including the licence boundary between engine and game code
- [Source layout](docs/dev/source-layout.md): where new code and vendored libraries go
- [The rebase plan](docs/dev/prey-rebase/plan.md): how openPR06 moved onto openQ4 and adapts upstream engine changes
- [Platform support](docs/dev/platform-support.md): what each platform needs before openPR06 claims it
- [CONTRIBUTING.md](.github/CONTRIBUTING.md): project conventions for pull requests

## Credits

**themuffinator** leads openPR06's development, with stewardship from **DarkMatter Productions**. openPR06 stands on **Human Head Studios**' PREY and its SDK, and on the **[openQ4](https://github.com/themuffinator/openQ4)** engine, which brings the work of everyone credited there with it.

<details>
<summary><b>Full credits and third-party software</b></summary>

- **themuffinator**: openPR06 development and maintenance; author of openQ4
- **DarkMatter Productions**: project stewardship and [website](https://www.darkmatter-quake.com)
- **Human Head Studios**: PREY (2006) and the PREY SDK that openPR06's game module is built from
- **[openQ4](https://github.com/themuffinator/openQ4)**: the engine openPR06 is built on; its engine history is merged into this repository
- **[Emile Belanger (emileb)](https://github.com/emileb)**: original author of the Android port, SigmaTouch integration, OpenGL ES 3.0 renderer, GLES shader variants, ETC2/EAC compression and associated mobile work, carried into openPR06 with the openQ4 engine; see [CONTRIBUTORS.md](CONTRIBUTORS.md) and the [integration record](docs/dev/android-gles-integration.md)
- **Justin Marshall**: Quake4Doom and early BSE reverse-engineering reference work
- **Daniel Gibson and the [dhewm3](https://github.com/dhewm/dhewm3) contributors**: cinematic JPEG handling inherited through Quake4Doom and openQ4, and the `dds/` texture-replacement naming convention openPR06 follows
- **Robert Beckebans**: renderer modernization reference work, including RBDOOM-3-BFG inspiration
- **id Software's official Doom 3 and Doom 3 BFG source releases**: retained idTech 4 source lineage; see the [source-provenance inventory](docs/dev/source-provenance.md)
- **id Software** and **Raven Software**: idTech 4 and the Quake 4 codebase behind openQ4
- **[Ronald Rivest and RSA Data Security, Inc.](https://www.rfc-editor.org/rfc/rfc1320)**: the MD4 Message-Digest Algorithm behind the PK4 compatibility checksums; [notice](LICENSES/MD4.txt)
- **[The RmlUi Team, CodePoint, Shift Technology and contributors](https://github.com/mikke89/RmlUi/tree/6.3)**: MIT-licensed layout library behind the engine's retained interface; [licence notice](LICENSES/RmlUi.txt)
- **[Baptiste Lepilleur and the JsonCpp authors](https://github.com/open-source-parsers/jsoncpp/tree/1.9.8)**: JSON parser for the retained interface's source files, used under its MIT option; [licence notice](LICENSES/JsonCpp.txt)
- **[Mikko Mononen, Eric Veach and the libtess2 contributors](https://github.com/memononen/libtess2/tree/8dbd6483e920311a58c9af10a10beb278efebc36)**: SGI-B-2.0 polygon tessellator for the retained interface's vector shapes; [licence notice](LICENSES/libtess2.txt)
- **Sam Lantinga and the [SDL contributors](https://github.com/libsdl-org/SDL)**: cross-platform window, display and input support; SDL uses the zlib licence
- **Chris Robinson and the [OpenAL Soft](https://openal-soft.org/) contributors**: the OpenAL runtime bundled with Windows builds
- **[Xiph.Org Foundation](https://xiph.org/vorbis/)**: libogg and libvorbis audio codecs
- **Jean-loup Gailly, Mark Adler and the [zlib-ng contributors](https://github.com/zlib-ng/zlib-ng/tree/2.3.3)**: zlib-licensed inflate and CRC-32 for reading PK4 packs, with run-time SIMD selection; [licence notice](LICENSES/zlib-ng.txt)
- **Sean Barrett and the [stb_vorbis contributors](https://github.com/nothings/stb/blob/master/stb_vorbis.c)**: MIT/public-domain Vorbis decoding
- **The [GLEW](https://github.com/nigels-com/glew) team**: OpenGL extension loading
- **The Khronos Group and [glslang contributors](https://github.com/KhronosGroup/glslang/tree/16.6.0)**: GLSL-to-SPIR-V compiler used by the Vulkan renderer; [licence notice](LICENSES/glslang.txt)
- **[The Khronos Group, Valve Corporation and LunarG](https://github.com/KhronosGroup/OpenXR-SDK/tree/release-1.1.65)**: Apache-2.0 OpenXR loader and headers carried with the engine; [licence notice](LICENSES/openxr-sdk.txt)
- **[The Khronos Group](https://github.com/KhronosGroup/OpenGL-Registry)**: MIT-licensed OpenGL ES API headers, with their original notices retained; [licence notice](LICENSES/KHRONOS-GLES-MIT.txt)

</details>

## Licence

The openPR06 engine is licensed under the [GNU General Public License v3.0](LICENSE). Files that keep Doom 3 or Doom 3 BFG Edition headers also keep their upstream notices and [Additional](LICENSES/DOOM-3-ADDITIONAL-TERMS.txt) [Terms](LICENSES/DOOM-3-BFG-ADDITIONAL-TERMS.txt); the [source-provenance inventory](docs/dev/source-provenance.md) records their scope and lineage.

The game code in [`src/game`](src/game/), [`src/Prey`](src/Prey/) and [`src/preyengine`](src/preyengine/) is derived from the PREY SDK and remains under [Human Head Studios' SDK EULA](LICENSES/PREY-SDK-EULA.rtf). [LICENSING.md](LICENSING.md) explains each component's terms, including the open question about distributing them linked together. PREY's game assets remain the property of their rights holders and are not included.

openPR06 is an independent project and is not affiliated with, endorsed by or sponsored by Human Head Studios, 2K, Bethesda, ZeniMax Media, id Software or Raven Software. PREY is a trademark of ZeniMax Media Inc.

---

<p align="center">
  <a href="https://www.darkmatter-quake.com">Website</a> · <a href="https://github.com/themuffinator/openPR06/releases">Releases</a> · <a href="https://github.com/themuffinator/openPR06/issues">Issues</a> · <a href="https://discord.gg/T32mFejwR4">Discord</a> · <a href="#top">Back to top</a>
</p>
