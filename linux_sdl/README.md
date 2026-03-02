# linux_sdl (PortMaster / RGDS)

This folder contains an SDL2 + OpenGL ES 3 frontend intended for PortMaster devices.

## Runtime files

- Place `yokoi_pack_rgds.ykp` next to the executable (working directory).
- Saves/settings are written to `./saves/` by default.

## Build (desktop Linux)

```sh
cmake -S linux_sdl -B build-linux
cmake --build build-linux -j
```

## Build on Windows (via WSL2)

This target links against Linux EGL/OpenGL ES + SDL2, so the supported way to
build it on Windows is to use **WSL2** (Ubuntu/Debian).

### 1) Install WSL2

In an elevated Windows PowerShell:

```powershell
wsl --install
```

Then install an Ubuntu distro from the Microsoft Store (if it doesn’t install
automatically), and open an **Ubuntu (WSL)** terminal.

### 2) Install dependencies (inside WSL)

From the repo root inside WSL:

```sh
make -f Makefile.linux_sdl deps
```

That installs:
- a C++ toolchain (`build-essential`)
- CMake + Ninja
- `pkg-config`
- SDL2 dev headers
- libpng dev headers
- Mesa EGL + GLES dev headers

If you prefer to install manually, the equivalent `apt` command is:

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config \
	libsdl2-dev libpng-dev libegl1-mesa-dev libgles2-mesa-dev
```

### 3) Build (inside WSL)

```sh
make -f Makefile.linux_sdl build
```

Binary output:

- `linux_sdl/build/yokoi_sdl`

### 4) Run (inside WSL)

By default the makefile runs using the RGDS pack in `CONVERT_ROM/`:

```sh
make -f Makefile.linux_sdl run
```

Or specify a pack path explicitly:

```sh
make -f Makefile.linux_sdl run PACK=../CONVERT_ROM/yokoi_pack_rgds.ykp
```

Notes:
- On Windows 11, WSLg usually supports opening the SDL window automatically.
- If you don’t get a window, you may need an X server on Windows or to run headless.

## PortMaster notes

You will typically cross-compile for `aarch64` and provide a launch script that
sets the working directory to the port folder.
