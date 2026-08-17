# SPLed on Zephyr

This is the SPLed product line built with Zephyr instead of yanga or spl-core.

It is the third leg of a three-way comparison of SPL approaches. The other two
are [avengineers/SPLed](https://github.com/avengineers/SPLed) (spl-core) and
[cuinixam/SPLed](https://github.com/cuinixam/SPLed) (yanga). All three build the
same product line, so the differences are about the framework and not about the
software.

This repository contains no product code. The components, the feature model and
the variant definitions stay in `cuinixam/SPLed`, which is pulled in as a west
project. Everything here is the cost of putting that product line on Zephyr.

## Working in the dev container

`native_sim` only builds on Linux, so on macOS and Windows the work happens in a
container. `.devcontainer/devcontainer.json` has the toolchain already.

In Zed, open this folder and accept the dev container prompt, or run
`project: open remote` from the command palette. Zed drives the official
`devcontainer` CLI, so it has to be installed first:

```bash
npm install -g @devcontainers/cli
```

The west workspace root is this repository itself, so everything west clones
lands under the mounted folder and the container needs no writable directory
above it.

Without Zed, the same environment is one command:

```bash
podman run --rm -it -v "$PWD":/spledz mcr.microsoft.com/devcontainers/base:debian bash
```

## Installing the tools

Zephyr expects cmake, ninja and a compiler to be on your PATH already. This
repository installs them with pypeline and poks, the same way the yanga port
does, so a fresh clone needs only Python and the libc headers the compiler
builds against. `dtc` is not among them: Zephyr runs it only if it happens to be
installed, for extra warnings, and does the real devicetree work in Python.

```bash
pipx install pypeline-runner
pypeline run                     # venv + tools + west workspace, then builds the default pair
source .venv/bin/activate
source build/install/env_setup.sh
```

`pypeline run` ends with a build, so on macOS give it a platform that builds
there — `pypeline run -i platform=esp32h2` — or stop after provisioning with
`pypeline run --step FetchEspressifBlobs`.

The tools are installed as one set, not per platform: `poks.json` is a single
manifest, so a machine that only ever builds `sim` still gets the RISC-V
toolchain. Making that per-platform is what `yanga-core` adds on top of
pypeline, and this repository deliberately has no yanga in it.

This layer is not part of the comparison. Zephyr has no answer for provisioning
a toolchain, and neither does spl-core, so the same mechanism is used here as in
the yanga port to keep the three legs comparable.

## Getting a workspace

`pypeline run` already did this: the `SetupWestWorkspace` step creates the
workspace and clones the projects. By hand it is:

```bash
mkdir -p .west
west config --local manifest.path .   # this repository is the workspace root
west config --local manifest.file west.yml
west update                           # clones zephyr, spled and hal_espressif into it
```

Either way you get:

```
spledz/
├── .west/           workspace marker and local config
├── app/             the Zephyr application, all the glue there is
├── deps/
│   ├── spled/       the product line, unmodified
│   └── zephyr/
└── modules/         modules imported by name from Zephyr's manifest
    └── hal/espressif/
```

The checkouts sit under `deps/` rather than at the root because this repository is
the workspace root and therefore a west project itself, and Zephyr treats any
project containing `zephyr/CMakeLists.txt` plus `zephyr/Kconfig` as a module. A
Zephyr checkout at the root would make this repository look like a module
wrapping Zephyr, and the Kconfig parse would recurse.

The price is that Zephyr can no longer find itself. Its in-tree package search
looks for a directory named `zephyr` among the application's ancestors, which is
the same path the module scan objects to, so `ZEPHYR_BASE` has to say where Zephyr
is. The `ZephyrSetup` step (`steps/zephyr_setup.py`) sets it, along with the
toolchain variant, and `pypeline run` writes both into
`build/install/env_setup.sh`. Sourcing that script is all any environment needs,
which is why neither the dev container nor the CI workflow declares them.

Zephyr's two discovery rules both assume the workspace root is not your
repository. One local step is what it costs to disagree with them.

`west init -l .` is the usual command and it is deliberately not used here. It
always creates `.west` *next to* the manifest repo, so the workspace root ends up
outside this repository and takes `deps/` with it. Setting
`manifest.path` directly keeps everything inside one directory, which is what a
container mount, an `rm -rf`, and a `.gitignore` can all reason about. The cloned
trees are ignored, so `git status` stays clean.

## Building the variants

A build is a **variant** and a **platform**, and nothing else has to be said:

```bash
pypeline run -i variant=disco -i platform=sim        # build/sim/disco
pypeline run -i variant=sleep -i platform=esp32h2    # build/esp32h2/sleep
./build/sim/disco/zephyr/zephyr.exe
```

Variants are the `prj_<variant>.conf` files: `disco`, `sleep`, `spa`. Platforms
are declared in `pypeline.yaml` under the `ZephyrBuild` step:

| Platform | Board | Toolchain | Where it runs |
| --- | --- | --- | --- |
| `sim` | `native_sim/native/64` | host compiler | Linux only |
| `esp32h2` | `esp32h2_devkitm/esp32h2` | `riscv64-zephyr-elf`, installed by poks | anywhere, macOS included |

SPLed has a platform layer; Zephyr has boards, toolchains and devicetree
overlays with nothing tying them together, so that table is where the layer
went. It is deliberately thin: because a platform is 1:1 with a board, naming
the board is enough for Zephyr to find the rest by itself, under `app/boards/`.
Two platforms sharing one board would need a board definition of their own, not
a bigger table.

Nothing stops you from building by hand, and the board-keyed files still apply:

```bash
west build -b native_sim/native/64 -d build/sim/disco app -- -DFILE_SUFFIX=disco
```

For the hardware platform the hand-typed form also needs the toolchain flags the
step passes (`ZEPHYR_TOOLCHAIN_VARIANT`, `CROSS_COMPILE`,
`CROSS_COMPILE_TOOLCHAIN_PATH`, `SYSROOT_DIR`) — see `steps/zephyr_build.py`.
They are passed per build rather than exported, because Zephyr caches them in
each build directory and gives environment variables the lowest precedence.

`native_sim` on its own builds a 32-bit binary and then needs a 32-bit libc, so
the `/native/64` board target is used instead.

Each build gets its own directory under `build/<platform>/<variant>`, because
pypeline also writes under `build/` and because a shared directory would
reconfigure on every switch.

## What belongs to a board

Anything that is true of the hardware rather than of the product lives in
`app/boards/`, which Zephyr searches by board name — `.overlay` for devicetree,
`.conf` for Kconfig, both merged **on top of** `prj_<variant>.conf` rather than
replacing it:

```
app/boards/
├── native_sim_native_64.overlay    second UART for the LED display, 3 emulated buttons
├── native_sim_native_64.conf       keep the shell on stdin/stdout
├── esp32h2_devkitm.overlay         WS2812 on GPIO8, 3 buttons on free header pins
└── esp32h2_devkitm.conf            CONFIG_LED_STRIP=y
```

The file names are the board target with `/` replaced by `_`. The short form
(`native_sim.conf`) is only accepted for boards that define a single SoC, which
`esp32h2_devkitm` does and `native_sim` does not — so the sim files carry the
full qualifiers and the H2 files do not. A name that matches nothing is not an
error; it is simply never applied, which is a quiet way to lose an overlay.

## Driving it

The application boots powered off, exactly like the other two ports. In SPLed's
`pc_terminal` platform you press `P` to switch it on and the arrow keys to change
the blink frequency or the brightness. There is no keyboard here: the buttons are
devicetree `gpio-keys` on `native_sim`'s emulated GPIO, and nothing drives them.
The shell takes that role:

```
uart:~$ spled power     # on, and again for off
uart:~$ spled up        # faster blinking (Disco) or brighter (Sleep)
uart:~$ spled down
```

Each command drives the emulated pin low for twenty task periods, because
`powerButton()` debounces over ten. `pc_terminal` solves the same problem by
holding a keypress for fifteen frames.

`native_sim` normally gives the console UART its own pseudo terminal, which puts
both the shell and the LED somewhere you are not looking. The variant configs set
`CONFIG_UART_NATIVE_PTY_0_ON_STDINOUT=y` so it lands in the terminal that started
the binary. Zephyr discourages that for heavy interactive shell use, since history
search and autocomplete misbehave, but typing these three commands is fine. To get
the full shell instead, drop the option and open the reported pseudo terminal in a
second window with `picocom /dev/pts/N`.

The LED gets a **second terminal**. On startup the binary prints something like:

```
uart connected to pseudotty: /dev/pts/3
```

Open it in a second terminal and you have the `pc_terminal` display, a block whose
background is the RGB value, redrawn in place whenever the colour changes:

```bash
picocom /dev/pts/3        # the number changes every run; Ctrl-A Ctrl-X to quit
```

For Disco that is green alternating with black, which is the blink. The shell
stays in the window you started the binary from.

`picocom` (installed by the dev container) rather than `screen`: it relays the
bytes untouched, so the 24-bit colour escape reaches the terminal. `screen`
re-renders and quantises the colour away, leaving a grey block that never appears
to change, and `cat` does not open a pseudo terminal slave reliably. The RGB
values are printed next to the block as a fallback, so the state stays readable
even where the colour is not.

Two terminals rather than one because the console is shared here. `pc_terminal`
owns its terminal and can redraw with a bare `\r`; on Zephyr the console belongs
to the shell, and interleaving a repainting display with a line editor ruins
both. Enabling the board's second UART (`app/boards/native_sim_native_64.overlay`)
gives the display a screen
of its own and the problem disappears.

The build itself is plain west. Nothing from pypeline, poks or yanga takes part
in it.

## What the port costs

The parts that carry over for free:

- **The feature model.** SPLed already uses KConfig and so does Zephyr, so
  `app/Kconfig` just sources SPLed's `KConfig` and the whole model appears.
- **The components.** They compile unmodified. They include `autoconf.h` by
  name, and Zephyr generates a header with that name, so the feature symbols
  resolve without a shim. The only CMake needed is one `target_sources` list.

The parts that do not:

- **The variant model.** SPLed enumerates its variants under `variants/`. Zephyr
  has nowhere to put that, so each variant becomes a `prj_<name>.conf` selected
  with `FILE_SUFFIX` at build time. Nothing in the source tree says which
  suffixes are valid. The list only exists in CI, or in this README.
- **Shared variant configuration.** `FILE_SUFFIX` replaces `prj.conf` instead of
  extending it, so every variant file repeats the same Zephyr settings.
- **Per-variant component lists.** In SPLed a variant names the components it is
  built from. Here that selection has to be re-derived from a feature symbol
  with `target_sources_ifdef`.
- **The platform layer.** SPLed abstracts hardware with a header-only interface
  component plus a per-platform adapter. Zephyr describes hardware in
  devicetree, so the adapters were rewritten against `gpio-leds`, `gpio-keys`
  and `led_strip`. This is the only real rewrite in the port. The interface
  itself held: `led_interface.h` is unchanged across a simulated on/off LED and
  a WS2812 on real silicon.
- **The word "platform" itself.** Zephyr has a board, a toolchain and a set of
  Kconfig fragments, with nothing naming the combination. The `ZephyrBuild` step
  config is where SPLed's platform concept ended up — outside Zephyr, because
  there is no place for it inside.
- **The way a person drives it.** A platform is not only adapters. `pc_terminal`
  is also a keyboard simulator with its own hardware-latching emulation and an
  ANSI renderer, and none of that has a Zephyr equivalent. The renderer moved
  over unchanged, but the input side had to be rebuilt as emulated GPIO behind a
  shell command (`app/src/buttons_shell.c`).
- **Compiler settings that came from the platform.** SPLed's toolchain files
  define `SPLE_TESTABLE_STATIC`. Zephyr has no platform layer to carry it, so
  the definition is repeated in `app/CMakeLists.txt`.

## What is not ported

- **The Arduino platforms.** SPLed targets the Uno R3 and the classic Nano, both
  AVR. Zephyr has no AVR architecture, so those targets cannot be ported at all.
  A hardware leg would have to move to a supported board such as
  `arduino_uno_r4` or `arduino_nano_connect`.
- **The gtest platform.** In SPLed, tests are a platform in the variant matrix.
  Zephyr tests are a separate tool (twister) with its own matrix, so the concept
  does not transfer.
- **The Base/Dev variant.** It carries example components rather than the
  product, so there is nothing to build here.

## Status

**`sim`** — all three variants build and run. Verified in a Debian container
against Zephyr v4.4.0, from `west init` through `west build` to a booting
binary. Each variant produces its own feature set in Zephyr's generated
`autoconf.h`, and the Sleep and Spa builds compile one component more than
Disco, which is the conditional `brightness_controller` doing its job.

**`esp32h2`** — all three variants build and link, on macOS, with no Zephyr SDK
installed: the `riscv64-zephyr-elf` toolchain comes from poks like every other
tool. 143 KB of flash and 66 KB of RAM for Disco. Not yet flashed to the board,
so nothing here is confirmed to run.

`native_sim` only runs on Linux, and only builds there — the POSIX architecture
refuses to configure anywhere else. On Windows or macOS use a container or a VM
for that platform; the hardware platform needs neither, which makes it the more
portable of the two. The CI workflow covers Linux.
