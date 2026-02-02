# Ergo Compiler

Ergo is an experimental programming language and this repository contains its reference compiler, written in Python. The compiler performs lexing, parsing, type checking, and code generation for the Ergo language.

## Features

- **Lexer**: Tokenizes Ergo source code.
- **Parser**: Builds an abstract syntax tree (AST) from tokens.
- **Type Checker**: Ensures type safety and reports errors.
- **Code Generator**: Emits C code from Ergo programs.
- **Modular Design**: Easy to extend and modify.
- **GPLv3 Licensed**: Open source and free to use.

## Project Structure

```
Ergo/
├── src/
│   └── ergo/
│       └── main.py     # Main compiler implementation
├── tests/              # Unit and integration tests
├── examples/           # Example Ergo programs
├── build.sh            # Convenience script
├── README.md           # This file
├── LICENSE             # GPLv3 license
├── .gitignore          # Git ignore rules
└── requirements.txt    # Python dependencies
```

## Getting Started

### Prerequisites

- Python 3.8 or newer

### Installation

1. Clone the repository:
    ```
    git clone https://github.com/yourusername/ergo.git
    cd ergo
    ```

2. Install dependencies:
    ```
    pip install -r requirements.txt
    ```

### Usage

To compile an Ergo source file:

```
PYTHONPATH=src python -m ergo.main path/to/source.e
```

Emit C:

```
PYTHONPATH=src python -m ergo.main path/to/source.e --emit-c out.c
```

Compile to native and run (produces `./run` with `-O3`):

```
PYTHONPATH=src python -m ergo.main run path/to/source.e
```

Using the build script:

```
./build.sh run path/to/source.e
./build.sh pyinstaller
```

After `pyinstaller`, use the standalone binary:

```
./dist/ergo run path/to/source.e
./dist/ergo path/to/source.e --emit-c out.c
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

3. Run the test suite:
    ```
    python -m unittest discover tests
    ```

4. Commit and push your changes, then open a pull request.

Please ensure your code adheres to the project's style and passes all tests.

## License

This project is licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE) for details.

## Contact

For questions or suggestions, please open an issue or contact the maintainer.

---
Happy hacking with Ergo!
