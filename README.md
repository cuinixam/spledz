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

Inside the container, the west workspace is created next to this repository at
`/workspaces`, so `zephyr/` and `spled/` live in the container rather than on
your host.

Without Zed, the same environment is one command:

```bash
podman run --rm -it -v "$PWD":/spledz mcr.microsoft.com/devcontainers/base:debian bash
```

## Installing the tools

Zephyr expects cmake, ninja and a compiler to be on your PATH already. This
repository installs them with pypeline and poks, the same way the yanga port
does, so a fresh clone needs nothing preinstalled but Python.

```bash
pipx install pypeline-runner
pypeline run                     # venv + cmake, ninja, gcc from poks.json
source .venv/bin/activate
source build/install/env_setup.sh
```

This layer is not part of the comparison. Zephyr has no answer for provisioning
a toolchain, and neither does spl-core, so the same mechanism is used here as in
the yanga port to keep the three legs comparable.

## Getting a workspace

```bash
west init -l .                   # this repository is the manifest repo
west update                      # clones zephyr and spled next to it
```

That gives you:

```
spledz-workspace/
├── spledz/     this repository (manifest repo)
├── spled/      the product line, unmodified
└── zephyr/
```

## Building the variants

```bash
export ZEPHYR_TOOLCHAIN_VARIANT=host
west build -b native_sim/native/64 -d build/disco app -- -DFILE_SUFFIX=disco
west build -b native_sim/native/64 -d build/sleep app -- -DFILE_SUFFIX=sleep
west build -b native_sim/native/64 -d build/spa   app -- -DFILE_SUFFIX=spa
./build/disco/zephyr/zephyr.exe
```

`native_sim` on its own builds a 32-bit binary and then needs a 32-bit libc, so
the `/native/64` board target is used instead. `ZEPHYR_TOOLCHAIN_VARIANT=host`
tells Zephyr to use the host compiler rather than look for the Zephyr SDK.

Each variant gets its own build directory. `-d` is needed because pypeline also
writes under `build/`, and because a shared directory would reconfigure on every
variant switch.

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
  devicetree, so the adapters were rewritten against `gpio-leds` and
  `gpio-keys`. This is the only real rewrite in the port.
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

All three variants build and run. Verified in a Debian container against Zephyr
v4.4.0, from `west init` through `west build` to a booting binary. Each variant
produces its own feature set in Zephyr's generated `autoconf.h`, and the Sleep
and Spa builds compile one component more than Disco, which is the conditional
`brightness_controller` doing its job.

`native_sim` only runs on Linux. On Windows or macOS use a container or a VM.
The CI workflow covers Linux; macOS and Windows would need a different board.
