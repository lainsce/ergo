# Ergo Compiler

Ergo is an experimental programming language and this repository contains its reference compiler, `ergo`, written in C.  
The compiler performs lexing, parsing, type checking, and code generation for the Ergo language.

## Installation

1. Clone the repository:

```sh
git clone https://github.com/name/ergo.git
cd ergo
```

### Usage

Build the compiler:

```sh
meson setup _build
meson compile -C _build
cd _build
sudo ninja install
```

Compile an Ergo source file:

```sh
ergo path/to/source.ergo
```

Compile to native and run:

```sh
ergo run path/to/source.ergo
```

### Example

See `examples/hello.ergo` for a simple Ergo program.

## Cogito GUI

Cogito is a separate shared library inside this repository. Build it once, then
you can run GUI examples without extra flags (assuming raylib is installed in a
standard location).

Build Cogito:

```sh
meson setup cogito/_build cogito
meson compile -C cogito/_build
cd _build
sudo ninja install
```

Run a GUI example:

```sh
ergo run cogito/examples/gui_gallery.ergo
```

## Contributing

Contributions are welcome! To contribute:

- Fork the repository and create your branch:

```sh
git checkout -b feature/your-feature
```

- Make your changes.
- Commit and push your changes, then open a pull request.
