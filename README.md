# Ergo Compiler

Ergo is an experimental programming language and this repository contains its reference compiler, written in C. The compiler performs lexing, parsing, type checking, and code generation for the Ergo language.

## Features

- **Lexer**: Tokenizes Ergo source code.
- **Parser**: Builds an abstract syntax tree (AST) from tokens.
- **Type Checker**: Ensures type safety and reports errors.
- **Code Generator**: Emits C code from Ergo programs.
- **Modules**: Namespaced `bring` imports with a small standard library.
- **Docs + Examples**: Language reference, quickstart guide, and sample programs.
- **Modular Design**: Easy to extend and modify.
- **GPLv3 Licensed**: Open source and free to use.

## Project Structure

```
Ergo/
├── src/
│   └── ergo/           # Main compiler implementation (C)
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
meson setup build
meson compile -C build
```

Compile an Ergo source file:

```
./build/ergo path/to/source.e
```

Compile to native and run:

```
./build/ergo run path/to/source.e
```

Environment variables:

```
ERGO_STDLIB=src/ergo/stdlib
ERGO_RUNTIME=src/ergo/runtime.inc
CC=cc
ERGO_CC_FLAGS="-O3 -std=c11 -pipe"
```

### Example

See `examples/hello.e` for a simple Ergo program.

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
