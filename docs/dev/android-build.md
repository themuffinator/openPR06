# Android, GLES and Sigma Touch

> **Development status:** This document describes the current source tree. openPR06 has not published an Android package; see [openPR06 status](#openpr06-status).

The Android platform, GLES renderer and Sigma Touch adapter originate in
[emileb's Android branch](https://github.com/emileb/openQ4/tree/android) and
[GLES shader variants branch](https://github.com/emileb/openQ4/tree/gles-shader-variants).
Credit for this port belongs to **emileb**. openQ4 adapts his implementation to
the current engine, renderer API, multiplayer module and package checks, and
openPR06 adapts openQ4's host to PREY (2006).

## openPR06 status

As of 22 November 2026 the Android build produces a verified arm64 APK for
PREY: a push-triggered CI probe of `.github/workflows/android-build.yml`
(`signing: ci`) ran `tools/build/build_android_release.sh` (Meson native build,
Gradle `assembleRelease`), signed the APK with a throwaway key and passed
`tools/build/verify_android_apk.py`. The same probe passed again on 24 November
2026 at `03498bae0`, with the `android-host` checks. What changed from openQ4's
host:

- The GLES module (`librenderer-gles_arm64.so`) supplies its own
  `R_ARBProgramUsesInteractionInputs`, `R_LoadARBProgramsForStartup` and
  `R_PrecacheLevelMaterialPrograms` (`src/renderer/GLES/gles_Backend.cpp`):
  it classifies retail PREY interaction programs as the GL module does, and has
  no ARB programs to preload. It does not draw those interaction stages.
- The client library is `libopenpr06.so`, the game module the unified
  `libgame_arm64.so`, the content directory `basepr/`, and the application IDs
  `com.darkmatter.openpr06` (`.emulator`, `.quest`).
- Setup imports a PREY `base/` folder and checks it as the engine does: the
  CD/DVD layout (`pak000.pk4` to `pak004.pk4`) or the digital one
  (`pak_data.pk4`, `pak_sound.pk4`, `pak_en_v.pk4`, `pak_en_t.pk4`), with the
  checksums from `officialPk4s` in `src/framework/FileSystem.cpp`. Retail
  `pak_data.pk4` contains an empty entry stored as DEFLATED without a deflate
  stream, which Java's `ZipFile` cannot inflate; `Pk4Checksum` accepts that
  shape. `tools/tests/android_retail_data.py --retail-root <Prey>/base` passes
  on the digital layout.
- Setup uses the Prey menu faces (`fonts/menu.ttf`, Talon UI Condensed) and
  Interface Atlas colours.
- Upstream names stay where renaming would only break merges or external
  ABIs: the `com.darkmatter.openq4` Java package and class names, the
  `-Popenq4*` Gradle properties, the `OPENQ4_*` host environment variables, the
  `openq4-package-*` manifest files and the SigmaTouch `Quake4_*` bridge
  ([mobile/README.md](../../mobile/README.md)).
- Active validation (`openpr06-validation.yml`, job `android-host`) runs the
  Android host, touch and GLES checks openQ4's push verification runs. The
  native NDK build and APK verification run only in `android-build.yml`.
- The [player guide](../user/android-setup.md) is openQ4's, adapted. Gaps it
  records against openQ4: PREY's menus have no **Android setup** entry (the
  engine's `openAndroidSetup` command and `controls.touchLayout` state are
  unused); `PreyGame_InCinematic` is gated to false, so touch input is not held
  back during cinematics; and PREY's game module never sets
  `ui_touchCanReload` or `ui_touchCanZoom`, so the reload and zoom buttons stay
  hidden. The weapon wheel (`src/Prey/game_weaponwheel.cpp`) takes the touch
  wheel's hold, direction and cancel (`ui_radialTouchActive`, `ui_radialOwner`),
  untried on a device.

Remaining before a release lane: the signing secrets below, and every device
check. No openPR06 APK has been installed on a device.

### Release signing

`android-build.yml` signs with `signing: release` by default. openPR06 has no
release key, so that mode fails until the maintainer:

1. creates an Android release key (PKCS#12) and keeps an independent secure
   backup; every later update must be signed with the same key;
2. adds the repository Actions secrets `ANDROID_RELEASE_KEYSTORE_BASE64` (the
   keystore, base64-encoded), `ANDROID_RELEASE_STORE_PASSWORD`,
   `ANDROID_RELEASE_KEY_PASSWORD` and `ANDROID_RELEASE_KEY_ALIAS`;
3. commits the key's public certificate as
   `mobile/android/release-signing-cert.pem`
   (`keytool -exportcert -rfc -keystore <file> -alias <alias>`).

Then `manual-release.yml` can mirror openQ4's `android` job: call
`./.github/workflows/android-build.yml` with the release version and
content-pack artifact, `signing: release` and `secrets: inherit`, and require
its APK and dependency-source archive before publishing.

`signing: ci` builds without secrets: it signs with a key generated for that
job and names the APK `...-ci-signed-not-for-distribution.apk`. Use it to test
the lane (for example from a push-triggered probe workflow on a throwaway
`ci-probe/*` branch, since `workflow_dispatch` needs the workflow on `main`);
never publish its output.

## Supported build shape

Android targets `arm64-v8a`, Android API 24 or later, SDL3 and OpenGL ES 3.0.
The native application is `libopenpr06.so`. The renderer is
`librenderer-gles_arm64.so`; the in-tree Prey game library (`src/game/`,
`src/Prey/`, `src/preyengine/`) builds the unified `libgame_arm64.so` for both
single player and multiplayer. Both modes use `basepr/`.
Android has no dedicated-server target. Desktop defaults remain unchanged.

Meson is the engine's build entry point on Android as on desktop. The port's
older standalone engine CMake file is superseded by the current Meson source
lists, generated version/savegame headers and renderer/game export maps.
The engine embeds BSE, and the game module links its own idlib archive.

The [standalone Android application](../../mobile/android/README.md) packages
these native outputs using Gradle and SDLActivity. It supports ordinary SDL
gamepad, keyboard, mouse and menu input, plus the experimental first-party
gameplay controls described in the [player guide](../user/android-setup.md).
Sigma Touch remains an optional compatible external host. Device gameplay
remains a separate validation step.

## Toolchain and dependencies

Use the current openQ4 checkout. Canonical SDK game sources and matching engine
interfaces build together; neither archived companion repository is a build
input. Historical Android integration revisions are recorded in the
[integration evidence](android-gles-integration.md).

Use the [Android NDK](https://developer.android.com/ndk/downloads), Python 3,
Meson 1.6 or newer, Ninja and CMake. The cross build uses NDK r30 (30.0.16248370).
The cross-file generator follows Android's
[other build systems guidance](https://developer.android.com/ndk/guides/other_build_systems):
it invokes Clang directly with the target/API instead of Windows `.cmd` wrappers.
All native libraries must use the same NDK, ABI, API and shared C++ runtime.

Obtain SDL3 3.4.18 sources (the repository's pinned subproject, including its
patches) and OpenAL Soft 1.25.2 sources separately; OpenAL Soft 1.25 needs NDK r30's
libc++, as r27d lacked the ranges support it uses. SDL is zlib licensed; OpenAL Soft is LGPL licensed.
These are the existing engine dependencies, built for the target. Preserve their
license notices when distributing the libraries, and satisfy OpenAL Soft's
source/relinking requirements. See [SDL](https://github.com/libsdl-org/SDL) and
[OpenAL Soft](https://github.com/kcat/openal-soft).
The dependency helper also stages the notices for the shared NDK C++ runtime
and OpenAL's bundled fmt implementation.

The helper uses each dependency's own CMake build and installs into one prefix;
it neither replaces the engine's Meson build nor downloads sources implicitly.
From the repository root on Windows, for example:

```powershell
python tools/build/prepare_android_deps.py --ndk E:/_SOURCE/_CODE/android-ndk-r30 --sdl-source subprojects/SDL3-3.4.18 --openal-source E:/_SOURCE/_CODE/openal-soft-1.25.2 --prefix .tmp/android-build/deps --build-root .tmp/android-build/dependency-builds-1252
python tools/build/android_cross.py --ndk E:/_SOURCE/_CODE/android-ndk-r30 --api 24 --out .tmp/android-build/arm64.ini
powershell -ExecutionPolicy Bypass -File tools/build/meson_setup.ps1 setup builddir/android-arm64 --cross-file .tmp/android-build/arm64.ini -Dandroid_deps_root=E:/Repositories/openQ4/.tmp/android-build/deps -Dbuildtype=release
powershell -ExecutionPolicy Bypass -File tools/build/meson_setup.ps1 compile -C builddir/android-arm64 -j 8
powershell -ExecutionPolicy Bypass -File tools/build/meson_setup.ps1 install -C builddir/android-arm64 --no-rebuild --skip-subprojects
```

Adjust absolute paths for your checkout. On Linux/macOS, run the same Python
helpers and use `bash tools/build/meson_setup.sh setup`, `compile` and `install`
with the corresponding arguments above. The wrapper refreshes the in-tree
source inventory when sources change; use it for subsequent rebuilds too.
Use a separate build directory so desktop settings and staged binaries remain
available. Android builds use the same canonical game and engine headers.

The dependency prefix must contain `include/SDL3`, `include/AL`,
`lib/libSDL3.so`, `lib/libopenal.so` and `lib/libc++_shared.so`.
The helper enables flexible 16 KiB page sizes for dependency builds; the Meson
cross file requests 16 KiB ELF segment alignment for engine and modules.

## Packaging

openQ4's Manual Releases calls the reusable Android APK workflow and requires
its signed APK plus matching dependency source archive before publishing;
openPR06's does not yet (see [Release signing](#release-signing)). Android
retains an experimental support tier, independent of GitHub's overall
prerelease flag. See the standalone guide for signing and upgrade details.

Installation goes to `.install/android/arm64-v8a/`: native modules in `lib/`,
generated packs and `mod.json` in `basepr/`. Copy dependency runtime libraries
from the prefix alongside the native modules in the host's `jniLibs/arm64-v8a/`.
Use a host that exposes an extracted native library directory for runtime module
loading. Keep the application's SDL Java sources and native SDL library from the
same SDL release.

Meson also installs `android-build.json` with the ABI, package variant, actual
native minimum API and SDL version. The standalone Gradle host requires this file, uses its API for
the APK's minimum SDK and rejects a mismatched SDL source tree. APK version names
follow the staged mod metadata; see the standalone guide for custom version-code
numbering when publishing several builds of one engine version.

Package **both** generated packs and the matching `mod.json`. The engine checks
their checksums and required version. Do not reuse stale metadata or disable
checks to load a mismatched package. This integration retains the normal full
pack contents; emileb's reduced CMake-only pak1 recipe is not applied because the
current renderer and content can evolve independently.

Players must provide their own installed PREY `base/` PK4 assets. Retail assets
are not included in a build or an APK. The host must provide filesystem paths
that allow the engine to read those assets and write its save/configuration data.
Automated MP launches must explicitly pass `+set ui_autoJoin 1`; interactive
launches use `+set ui_autoJoin 0`.

## Diagnostic emulator build

The recovered `x86_64` target uses the same API 24 floor, SDL and GLES3 code,
with `_x64` renderer/game modules. It is a development target under AND-01,
separate from the experimental ARM64 release. Keep its dependency prefix,
native build and stage separate:

```powershell
python tools/build/prepare_android_deps.py --ndk E:/_SOURCE/_CODE/android-ndk-r30 --abi x86_64 --api 24 --sdl-source subprojects/SDL3-3.4.18 --openal-source E:/_SOURCE/_CODE/openal-soft-1.25.2 --prefix .tmp/android-emulator/deps --build-root .tmp/android-emulator/dependency-builds
python tools/build/android_cross.py --ndk E:/_SOURCE/_CODE/android-ndk-r30 --abi x86_64 --api 24 --out .tmp/android-emulator/x64.ini
$dependencyPrefix = (Resolve-Path .tmp/android-emulator/deps).Path
& tools/build/meson_setup.ps1 setup builddir/android-x64 --cross-file .tmp/android-emulator/x64.ini "-Dandroid_deps_root=$dependencyPrefix" --buildtype=release
& tools/build/meson_setup.ps1 compile -C builddir/android-x64 -j 4
& tools/build/meson_setup.ps1 install -C builddir/android-x64 --no-rebuild --skip-subprojects
```

Package that stage with `-Popenq4Abi=x86_64`,
`-Popenq4Stage=<checkout>/.install/android/x86_64` and `assembleDebug`, retaining
the matching SDL source argument and an absolute project-cache directory from
the standalone build instructions. Gradle writes
`builddir/android-apk-emulator/app/outputs/apk/debug/app-debug.apk`.
The application ID is `com.darkmatter.openpr06.emulator`, with its own private
saves and external files directory. Its localized launcher name identifies
the emulator build. No release APK or release bundle task is enabled for this
ABI; the phone release workflow and certificate policy remain unchanged.

For verification, pass `--variant emulator` to
`tools/build/verify_android_apk.py`, with the exact staged version, SDK build
tools and the public certificate for the diagnostic signing key. It requires a
debuggable emulator package, API 24, exactly six matching x86_64 libraries,
16 KiB alignment and the same content/notices checks. The default verifier
continues to require a nondebuggable ARM64 phone release and rejects this APK.
An ABI label alone does not qualify a binary: ELF machine/API and every
packaged library's bytes are checked.

## Physical device qualification

`tools/tests/android_device_qualification.py` is the AND-02 device harness. It is
opt-in and needs a USB-connected phone that is unlocked when each phase starts,
plus a debuggable APK. It sends no OS touch or keyboard input. To start the game
it sends setup the debuggable-only `diagnostic_launch` extra (`sp` or `mp`).
Setup still runs its own installed-data check first, and release builds ignore
the extra. A pending diagnostic launch or a `hold` keeps the screen on, so a
phone's display timeout cannot stall a run between phases. Setup also keeps the
screen on while it checks or extracts data.

```powershell
python tools/tests/android_device_qualification.py --adb "C:/Program Files (x86)/Android/android-sdk/platform-tools/adb.exe" --serial <serial> --phases campaign,sustain,mp --output .tmp/and02/<run> --check-budgets
```

The harness has three phases:

- `campaign` plays stock single-player maps in order: `hangar1`, `hangar2` and
  `mcc_landing` through each level's own end trigger, then fresh loads of
  `airdefense1`, `convoy1` and `process1`. It finishes with a save and reload on
  `process1`.
- `sustain` runs `--sustain-minutes` of view sweeps and sends one
  `RUNNING_CRITICAL` memory-trim request halfway through.
- `mp` runs a Q4DM1 listen server with `--bots` bots.

What each part records:

- **Every stage:** a frame trace, a timed view sweep, `listSounds` audio
  residency and a capture.
- **Each phase:** device, GLES and OpenAL device facts, and storage before and
  after.
- **Every 15 seconds:** thermal status, temperatures, app PSS and battery.

`--cold` moves the generated image cache aside for the campaign and restores
it afterwards. The harness preserves the player's config and log. It removes
every script, save, capture and trace it creates. `report.json` lands under the
given `.tmp` directory. With `--check-budgets` it compares the run against the
per-model limits in
[`android-device-budgets.json`](android-device-budgets.json).

## Meta Quest flavor

The standalone headset build is the phone build declared as an immersive
OpenXR app (AND-05). Configure an arm64-v8a build with `-Dandroid_headset=true`
(the build refuses x86_64): it stages package variant `quest`, installs the
`openq4-headset` marker and links the Khronos OpenXR loader into
`libopenpr06.so`. Package that stage with `-Popenq4Quest=true`:

```sh
gradle -p mobile/android --project-cache-dir=<checkout>/.tmp/android-gradle-project \
  -Psdl3SourceRoot=<SDL3-3.4.18> -Popenq4Stage=<quest stage> -Popenq4Quest=true assembleRelease
```

Gradle refuses a Quest stage without the property and the property without a
Quest stage, and builds under `builddir/android-apk-quest/`. The manifest is
generated from the phone manifest, so the two cannot drift: it adds the
head-tracking requirement, the OpenXR permissions and runtime-broker queries,
the supported headsets (Quest 2, Pro, 3 and 3S), focus awareness, the 2D
category that opens setup as a panel, and the Quest VR and Khronos immersive
categories on the game activity. When the marker asset is present,
`OpenQ4Activity` starts the game with `vr_enable 1`, the touch controls off,
the never-seen window mirror off, and the window kept landscape.

On Android the loader is upstream's JNI build: it finds the runtime through
the Khronos runtime broker, so the engine initializes it with the app's Java
VM and activity (`xrInitializeLoaderKHR`) and names them again at instance
creation (`XR_KHR_android_create_instance`). The session binds the GLES
renderer's EGL context through `XR_KHR_opengl_es_enable`. The GLES module
presents with the OpenGL presentation code: eyes and the virtual screen are
render textures behind the redirected default target, a blit copies them into
the swapchain images, and an sRGB image is written with
`GL_EXT_sRGB_write_control` off (Quest's Adreno has it) or, without it,
through a draw that decodes the engine's gamma-encoded pixels, since OpenGL
ES encodes every write into an sRGB image. Where the runtime offers them, the
engine chooses the refresh rate (`XR_FB_display_refresh_rate`,
`vr_refreshRate`), holds the CPU and GPU level (`XR_EXT_performance_settings`,
`vr_performanceLevel`, sustained high by default), logs thermal warnings, and
names the frame thread (`XR_KHR_android_thread_settings`).

`verify_android_apk.py --variant quest` then requires its own application ID,
`com.darkmatter.openpr06.quest` (never the phone's, so neither APK replaces the
other), a nondebuggable release signature, the same six ARM64 libraries,
the marker asset, and a manifest that requires head tracking, requests the
OpenXR runtime permission, opens the game with the immersive OpenXR and Quest
categories and names its supported headsets. Phone and emulator APKs must
carry none of these and no marker. No stage and no distributable APK may carry
the headset-free OpenXR test runtime (`libopenq4-xr-test-runtime.so`). The
application ID and API floor are the flavor's until AND-05's distribution
decision.

Phones carry no OpenXR: only the Quest flavor and the diagnostic x86_64
emulator build it. The emulator checks the headset path without a headset:
package its debug APK with `-Popenq4XrTestRuntime=<build>/libopenq4-xr-test-runtime.so`
(Gradle refuses the property for any other variant), then run
`android_emulator_smoke.py --mode vr --xr-runtime <that library>`. The harness
admits exactly that library through `verify_payload`, writes the runtime's
manifest, settings and script into the app's external files (the Android
loader reads no environment, so `vr_runtimeJson` reaches it as a loader
property, and the runtime reads `openq4_xr_test.env`), plays Air Defense 1 in
stereo with a scripted head turn, and requires the ES binding, the refresh
rate requests, the performance levels, the thermal report and lit, differing
eyes in the runtime's captures. It qualifies SwiftShader's GLES only:
Adreno's sRGB write control, Horizon OS's panel-to-immersive launch, the
controllers, comfort, session recovery across the system menu and sleep, and
thermal and refresh budgets need a headset.

Create AVDs using the [Android emulator tools](https://developer.android.com/studio/run/emulator-commandline).
Set `ANDROID_USER_HOME` and the AVD data path inside the checkout's `.tmp/`
before invoking SDK/AVD tools; keep SDK downloads and VM disks off the system
drive. Retain the emulator/image version, renderer and capture method with each
run. Emulator results qualify that virtual configuration only; they cannot
establish Adreno/Mali driver, physical touch, audible output or thermal behavior.
The image must expose GLES3 as well as the required Android API level. The
API 24 default x86_64 revision 8 image reports GLES2 with both SwiftShader and
the tested NVIDIA host renderer, so it cannot qualify gameplay at the API floor.
The runtime probe rejects that configuration before installing or copying data;
the APK's API 24 minimum is unchanged.

`tools/tests/android_emulator_smoke.py` is an opt-in native gameplay probe for a
booted, rooted AOSP/userdebug emulator. It verifies the diagnostic APK, copies
and hashes the caller's retail archives, then runs silent Hangar 1 movement,
look/fire/release and save/load checks. `--mode mp` instead runs a Q4DM1
listen server with explicit auto-join and the same movement/release checks;
it does not create a save. `--mode rotate` applies the Interface setup's
Landscape and portrait choice, turns the AVD to portrait mid-match through its
`user_rotation` setting when the engine asks, drives the portrait thumb deck,
turns back and saves/loads. It checks the window and engine captures follow
each turn, the deck arrangement and that neither turn moves the player, and it
restores the AVD's rotation settings and the app's setup preferences.
`--mode lifecycle` writes a touch layout editor record into the setup
preferences (as **Apply layout** does), checks that the engine places, sizes,
fades and hides the controls it names, then sends the app to the background
with the activity manager's Home intent, requires the `Suspended` save, ends
the backgrounded process with `am kill` as Android's memory management would,
relaunches it and loads that save, which must restore the player where the app
was left. `--mode resume --server <IPv4>:<port> --away <seconds>` joins a
dedicated server the emulator can reach, stays in the background for the given
time and returns the same process: a stay shorter than the 40-second time-out
must keep its connection, a longer one must reconnect at once. Captures come from the engine's
`screenshot` command. The probe starts the internal game Activity from the
rooted AVD, so it does not qualify setup navigation or physical input. It refuses
physical-device serials and an already running diagnostic app, preserves existing
settings, and removes its exact scripts/save slot/captures after collecting them:

```powershell
python tools/tests/android_emulator_smoke.py --adb <sdk>/platform-tools/adb.exe --serial emulator-5580 --apk builddir/android-apk-emulator/app/outputs/apk/debug/app-debug.apk --stage .install/android/x86_64 --build-tools <sdk>/build-tools/35.0.0 --certificate .tmp/emulator-debug-cert.pem --assets "E:/SteamLibrary/steamapps/common/Quake 4/q4base" --version <staged-version> --output .tmp/android-emulator/run-1
```

An identical installed APK is retained after full SHA-256 verification;
otherwise installation updates the diagnostic app without clearing its data.
The report includes a copy of the runner and the tested source/package hashes.
Use a fresh evidence directory for each run and inspect its engine captures
before accepting a visual result. Inspect a failed report and any retained test
files before retrying; the probe does not overwrite an existing diagnostic slot.

## Optional Sigma Touch host adapter

The default Android build uses SDL and has no Sigma Touch dependency. For a
legally compatible external host, set `-Dsigmatouch_root=<Clibs_OpenTouch>` and
`-Dsigmatouch_controls_root=<touchcontrols-include-root>`, and supply the target
`touchcontrols` and `saffal` libraries under `android_deps_root/lib`. This enables
`OPENQ4_SIGMATOUCH` and builds emileb's portable API and touch-layout adapters
without injecting the engine PCH into the host sources.

This external host also requires its compatible SDL3 callback extensions
(`SDL_beloko_extra.h` and the corresponding linked exports). Ordinary SDL3
dependency builds support the standalone Activity; they do not supply those
host-specific callbacks. Keep host headers, native SDL and touch libraries from
one compatible integration. The touch editor's labels are resolved after engine
initialization, since the host constructs its controls before starting openQ4.

External host dependencies are **not** included or licensed by this repository.
The examined MobileTouchControls license grants GPLv2 without an "or later"
clause, and the examined Clibs_OpenTouch tree has no top-level license grant.
Those versions cannot be assumed compatible with openQ4's GPLv3 licensing.
Distribution of a linked Sigma Touch build requires an appropriate compatible
license grant for those dependencies. The adapter option is not evidence that
such a grant exists, and does not enable or remove host validation checks.

## Desktop GLES development

On Linux with GLES/EGL development libraries, enable
`-Dbuild_renderer_gles=enabled` and launch with `+set r_renderApi gles`.
Windows and macOS may supply an ANGLE SDK through `-Dgles_root=<prefix>` with
link libraries under `lib/` and runtime libraries available to the loader.
Dependencies and renderer modules must have the same architecture.

The original `prepare_macos_angle.sh` helper remains a local diagnostic tool.
It extracts ANGLE from an installed Chrome; those binaries are not a
redistributable SDK. A GLES module using that implicit local staging directory
is never installed. Explicitly supplied ANGLE builds require their own license
notices and deployment; desktop GLES remains opt-in.

For runtime checks use windowed desktop launches and the engine's `screenshot`
command. Enter a map before claiming gameplay validation. Android cross-link
success does not verify the app lifecycle, device driver, touch host or gameplay.

The local push/commit validation profiles require a C++ compiler and JDK 17 or
21 (`javac`/`java` on `PATH`, or `JAVA_HOME`). Their Android host regression runs
the actual Activity against temporary filesystem fixtures without a device.
