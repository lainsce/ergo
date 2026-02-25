# Ergo

> Vala was the war. Ergo is the spoils.

This repository contains:

- `ergo`: the Ergo language compiler (C11)

## Requirements

- C compiler (`cc`) with C11 support
- Meson
- Ninja

Example (macOS/Homebrew):

```sh
brew install meson ninja
```

## Build Ergo

```sh
meson setup ergo/build ergo
meson compile -C ergo/build
```

Run the compiler from the build tree:

```sh
./ergo/build/ergo --help
./ergo/build/ergo ergo/examples/hello.ergo
./ergo/build/ergo run ergo/examples/hello.ergo
```

## Optional Install

Install Ergo and stdlib:

```sh
meson install -C ergo/build
```

## Useful Environment Variables

- `ERGO_STDLIB`: override stdlib path
- `ERGO_CACHE_DIR`: cache directory for compiled binaries
- `ERGO_NO_CACHE=1`: disable binary cache
- `ERGO_KEEP_C=1`: keep generated C files
- `ERGO_CC_FLAGS`: extra C compiler flags
- `NO_COLOR=1`: disable colored compiler output
