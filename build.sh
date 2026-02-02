#!/bin/sh
# build.sh - Convenience script for running and testing the Ergo compiler

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC="$PROJECT_ROOT/src/ergo/main.py"
CLI_WRAPPER="$PROJECT_ROOT/ergo_cli.py"
EXAMPLES_DIR="$PROJECT_ROOT/examples"
TESTS_DIR="$PROJECT_ROOT/tests"
PYTHON=${PYTHON:-python3}

usage() {
    echo "Ergo Compiler Build Script"
    echo ""
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  run <file.e>      Run the Ergo compiler on a .e file"
    echo "  test              Run the test suite"
    echo "  lint              Run flake8 linter on the source code"
    echo "  pyinstaller       Build a standalone ergo binary (requires pyinstaller)"
    echo "  help              Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 run examples/fizzbuzz.e"
    echo "  $0 test"
    echo "  $0 pyinstaller"
    echo ""
}

run_compiler() {
    if [ $# -lt 1 ]; then
        echo "Error: No input file specified."
        echo "Usage: $0 run <file.e>"
        exit 1
    fi
    PYTHONPATH="$PROJECT_ROOT/src${PYTHONPATH:+:$PYTHONPATH}" \
        $PYTHON -m ergo.main "$@"
}

run_tests() {
    if [ ! -d "$TESTS_DIR" ]; then
        echo "No tests directory found."
        exit 1
    fi
    $PYTHON -m unittest discover "$TESTS_DIR"
}

run_lint() {
    if ! command -v flake8 >/dev/null 2>&1; then
        echo "flake8 not found. Install with: pip install flake8"
        exit 1
    fi
    flake8 "$SRC"
}

run_pyinstaller() {
    if ! command -v pyinstaller >/dev/null 2>&1; then
        echo "pyinstaller not found. Install with: pip install pyinstaller"
        exit 1
    fi
    $PYTHON -m PyInstaller --onefile --name ergo --paths "$PROJECT_ROOT/src" \
        --add-data "$PROJECT_ROOT/src/ergo/stdlib:ergo/stdlib" \
        "$CLI_WRAPPER"
}

COMMAND="$1"
shift || true

case "$COMMAND" in
    run)
        run_compiler "$@"
        ;;
    test)
        run_tests
        ;;
    lint)
        run_lint
        ;;
    pyinstaller)
        run_pyinstaller
        ;;
    help|"")
        usage
        ;;
    *)
        echo "Unknown command: $COMMAND"
        usage
        exit 1
        ;;
esac
