# Ergo Compiler

Ergo is an experimental programming language and this repository contains its reference compiler, written in C. The compiler performs lexing, parsing, type checking, and code generation for the Ergo language.

## Features

- **Lexer**: Tokenizes Ergo source code.
- **Parser**: Builds an abstract syntax tree (AST) from tokens.
- **Type Checker**: Ensures type safety and reports errors.
- **Code Generator**: Emits C code from Ergo programs.
- **Modules**: Namespaced `bring` imports with a small standard library.
- **Cogito GUI**: A raylib-backed GUI library built as a shared library in `cogito/`.
- **Docs + Examples**: Language reference, quickstart guide, and sample programs.
- **Modular Design**: Easy to extend and modify.
- **GPLv3 Licensed**: Open source and free to use.

## Project Structure

```
Ergo/
├── src/
│   └── ergo/           # Main compiler implementation (C)
├── cogito/             # Cogito GUI library (shared lib + assets)
├── docs/               # Language reference + quickstart
├── examples/           # Example Ergo programs
├── meson.build         # Build metadata
├── README.md           # This file
├── LICENSE             # GPLv3 license
└── .gitignore          # Git ignore rules
```

## Getting Started

### Prerequisites

- A C compiler (clang/gcc)
- Meson + Ninja (or another Meson backend)

### Installation

1. Clone the repository:
    ```
    git clone https://github.com/yourusername/ergo.git
    cd ergo
    ```

### Usage

Build the compiler:

```
meson setup _build
meson compile -C _build
```

Compile an Ergo source file:

```
./_build/ergo path/to/source.ergo
```

Compile to native and run:

```
./_build/ergo run path/to/source.ergo
```

Environment variables:

```
ERGO_STDLIB=src/ergo/stdlib
ERGO_RUNTIME=src/ergo/runtime.inc
CC=cc
ERGO_CC_FLAGS="-O3 -std=c11 -pipe"
ERGO_RAYLIB_CFLAGS=...
ERGO_RAYLIB_FLAGS=...
ERGO_COGITO_CFLAGS=...
ERGO_COGITO_FLAGS=...

Note: if `cogito/include/cogito.h` and `cogito/_build/libcogito.*` are present,
`ergo run` will auto-add Cogito and raylib flags. Use the environment variables
above only to override defaults or non-standard install paths.
```

### Example

See `examples/hello.ergo` for a simple Ergo program.

## Cogito GUI

Cogito is a separate shared library inside this repository. Build it once, then
you can run GUI examples without extra flags (assuming raylib is installed in a
standard location).

Build Cogito:

```
meson setup cogito/_build cogito
meson compile -C cogito/_build
```

Run a GUI example:

```
./_build/ergo run cogito/examples/gui_kitchensink.ergo
```

If you want to use your system `ergo`, copy the rebuilt binary:

```
cp _build/ergo /opt/homebrew/bin/ergo
```

## Contributing

Contributions are welcome! To contribute:

1. Fork the repository and create your branch:
    ```
    git checkout -b feature/your-feature
    ```

2. Make your changes and add tests if applicable.

3. Run the test suite (when available).

4. Commit and push your changes, then open a pull request.

Please ensure your code adheres to the project's style and passes all tests.

## License

This project is licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE) for details.

## Contact

For questions or suggestions, please open an issue or contact the maintainer.

---
Happy hacking with Ergo!
