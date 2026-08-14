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
does, so a fresh clone needs nothing preinstalled but Python.

```bash
pipx install pypeline-runner
pypeline run                     # venv + cmake, ninja, gcc + the west workspace
source .venv/bin/activate
source build/install/env_setup.sh
```

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
west update                           # clones zephyr and spled into it
```

Either way you get:

```
spledz/
├── .west/           workspace marker and local config
├── app/             the Zephyr application, all the glue there is
└── deps/
    ├── spled/       the product line, unmodified
    └── zephyr/
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

```bash
west build -b native_sim/native/64 -d build/disco app -- -DFILE_SUFFIX=disco
west build -b native_sim/native/64 -d build/sleep app -- -DFILE_SUFFIX=sleep
west build -b native_sim/native/64 -d build/spa   app -- -DFILE_SUFFIX=spa
./build/disco/zephyr/zephyr.exe
```

`native_sim` on its own builds a 32-bit binary and then needs a 32-bit libc, so
the `/native/64` board target is used instead. `ZEPHYR_TOOLCHAIN_VARIANT=host`,
set by the `ZephyrSetup` step, tells Zephyr to use the host compiler rather than
look for the Zephyr SDK.

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
