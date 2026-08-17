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

`native_sim` only builds on Linux, so on macOS and Windows the `sim` platform
needs a container or a Linux VM. `.devcontainer/devcontainer.json` has the
toolchain already. A plain Ubuntu machine or VM works just as well and needs one
apt line, see "Installing the tools" below. The `esp32h2` platform needs neither:
it cross-compiles anywhere, macOS included, and flashing is a USB cable.

Do not use the container and the host on the same clone at the same time. They
share `.venv` through the mount, and whichever one ran `pypeline run` last owns
it, since the binaries in it are platform specific.

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

On a plain Ubuntu machine, with no container involved, the prerequisites are one
apt line:

```bash
sudo apt-get install -y git python3 python3-venv pipx libc6-dev picocom
pipx ensurepath                  # then open a new shell
```

`libc6-dev` is what the host compiler links against for the `sim` platform, and
`picocom` reads the board's serial console. Nothing else is needed. cmake,
ninja, gcc and the RISC-V toolchain all arrive with `pypeline run`.

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

## Flashing the board

Not done yet. The commands below come from the board's runner configuration and
from the vendor schematic, not from a board on a desk, so treat them as the
starting point rather than as a tested recipe.

```bash
source .venv/bin/activate             # west and esptool live here
source build/install/env_setup.sh
west flash -d build/esp32h2/disco
picocom /dev/ttyUSB0 -b 115200        # Ctrl-A Ctrl-X to quit
```

`west flash` uses the `esp32` runner, which is esptool underneath. esptool is
already in the venv, because the Espressif SoC CMake wants it at configure time
too. The runner picks the port itself, or takes `--esp-device /dev/ttyUSB0`, or
reads `ESPTOOL_PORT` from the environment. It flashes at 921600 baud and the
console runs at 115200.

Three things about this board in particular:

- **The port is a USB-UART bridge, not the SoC's own USB.** The board DTS puts
  console and shell on `uart0`, which is wired to the CH343 bridge, so the
  device is `/dev/ttyUSB0` rather than `/dev/ttyACM0`. The SoC also exposes a
  native USB serial peripheral on the same cable, but nothing routes the console
  there yet.
- **No button press to flash.** The board drives BOOT and reset from DTR and RTS,
  so esptool puts it into download mode on its own.
- **Serial access needs the `dialout` group.** `sudo usermod -aG dialout $USER`,
  then log out and back in. In a Parallels VM the device also has to be handed to
  the guest first, under Devices, USB & Bluetooth.

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

### On the board

Everything above is the simulator, and the same three commands work on the
board. Wiring the buttons is optional, which matters because the application
boots powered off: with nothing wired and no way in, the LED would just stay
dark.

```
uart:~$ spled power     # over picocom, on the board
uart:~$ spled state
```

Physical buttons and shell commands work at the same time, in any order. A press
from the shell reconfigures the pin as an output and drives the pressed level,
keeping the input enabled so the adapter still reads the pin; a release puts it
back to a plain input and lets the pull-up win. Since the shell only ever drives
the pin the same way a button does, holding a real button during a shell press
pulls the line the same direction instead of fighting the driver. The adapter
above cannot tell the two apart, and does not need to.

This is the part of the port with no SPLed counterpart. `pc_terminal` has a
keyboard simulator built into the platform; here the equivalent is a shell
command that drives a GPIO, and how it drives it depends on whether the pin is
emulated or real.

**Untested on hardware.** The emulated path is exercised by the sim platform.
The pin-driving path is read off the ESP32 GPIO driver, which enables the input
buffer and the output driver independently, but nothing has been flashed yet.

The pins are declared in `app/boards/esp32h2_devkitm.overlay`. They are active
low with the internal pull-up enabled, so a button simply connects its pin to
ground:

| Button | Pin |
| --- | --- |
| Power | GPIO10 |
| Up | GPIO11 |
| Down | GPIO12 |

BOOT on GPIO9 is left alone, since the flashing circuit uses it. The LED needs
no wiring: it is the onboard WS2812 on GPIO8 and it takes the RGB value
directly, so the colour block the simulator has to draw as text has no
counterpart here. The Zephyr shell still sits on the console for logs.

The compile itself is plain west, and nothing from poks or yanga takes part in
it. pypeline only chooses the variant and the platform and passes the toolchain
flags that go with them, which you can type yourself instead.

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

`native_sim` only runs on Linux, and only builds there. The POSIX architecture
refuses to configure anywhere else. On Windows or macOS use a container or a VM
for that platform; the hardware platform needs neither, which makes it the more
portable of the two.

CI builds the full matrix, three variants for both platforms, on Linux. The
simulator jobs run the binary for two seconds to prove it boots; the hardware
jobs check that a flashable image came out, because a runner cannot do more than
that without a board attached.
