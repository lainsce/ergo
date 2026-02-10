# Ergo + Cogito

This repository contains:

- `ergo`: the Ergo language compiler (C11)
- `cogito`: the GUI runtime used by Ergo GUI apps

## Requirements

- C compiler (`cc`) with C11 support
- Meson
- Ninja

For Cogito GUI builds:

- SDL3
- SDL3_ttf
- freetype2
- SDL3_image (optional, used for themed icon file loading)

Example (macOS/Homebrew):

```sh
brew install meson ninja sdl3 sdl3_ttf sdl3_image freetype
```

## Build Ergo

```sh
meson setup build
meson compile -C build
```

Run the compiler from the build tree:

```sh
./build/ergo --help
./build/ergo examples/hello.ergo
./build/ergo run examples/hello.ergo
```

## Build Cogito

Build the Cogito shared library in its own build directory:

```sh
meson setup cogito/build cogito
meson compile -C cogito/build
```

Then run GUI examples with Ergo:

```sh
./build/ergo run cogito/examples/gui_hello.ergo
./build/ergo run cogito/examples/gui_gallery.ergo
```

If your program uses Cogito, include:

```ergo
bring cogito
```

## Optional Install

Install Ergo and stdlib:

```sh
meson install -C build
```

Install Cogito library and headers:

```sh
meson install -C cogito/build
```

## Useful Environment Variables

- `ERGO_STDLIB`: override stdlib path
- `ERGO_CACHE_DIR`: cache directory for compiled binaries
- `ERGO_NO_CACHE=1`: disable binary cache
- `ERGO_KEEP_C=1`: keep generated C files
- `ERGO_CC_FLAGS`: extra C compiler flags
- `ERGO_COGITO_CFLAGS`: extra C flags for Cogito
- `ERGO_COGITO_FLAGS`: extra linker flags for Cogito
- `ERGO_RAYLIB_CFLAGS` / `ERGO_RAYLIB_FLAGS`: optional extra raylib flags (legacy compatibility path)
- `NO_COLOR=1`: disable colored compiler output
