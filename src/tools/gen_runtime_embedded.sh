#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ERGO_DIR="$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)"

RUNTIME_SRC="${1:-$ERGO_DIR/src/runtime.inc}"
OUT_FILE="${2:-$ERGO_DIR/src/runtime_embedded.h}"
TMP_FILE="${TMPDIR:-/tmp}/ergo_runtime_expanded.$$"

cleanup() {
  rm -f "$TMP_FILE"
}
trap cleanup EXIT INT TERM

awk '
function dirname(path,    out) {
  out = path
  sub(/[^\/]+$/, "", out)
  if (out == "") out = "./"
  return out
}
function emit(path,    line, q1, rest, q2, inc, base, full) {
  while ((getline line < path) > 0) {
    if (line ~ /^[ \t]*\/\/[ \t]*@include[ \t]*"/) {
      q1 = index(line, "\"")
      if (q1 > 0) {
        rest = substr(line, q1 + 1)
        q2 = index(rest, "\"")
        if (q2 > 0) {
          inc = substr(rest, 1, q2 - 1)
          base = dirname(path)
          full = base inc
          emit(full)
          print ""
          continue
        }
      }
    }
    print line
  }
  close(path)
}
BEGIN {
  emit(ARGV[1])
  ARGV[1] = ""
}
' "$RUNTIME_SRC" > "$TMP_FILE"

{
  echo "#ifndef ERGO_RUNTIME_EMBEDDED_H"
  echo "#define ERGO_RUNTIME_EMBEDDED_H"
  echo ""
  echo "// Auto-generated snapshot of ergo/src/runtime.inc with // @include expansion."
  echo "// Used as a fallback when runtime.inc is not available next to the ergo binary."
  echo "// Regenerate with: ergo/tools/gen_runtime_embedded.sh"
  echo ""
  echo "static const char ergo_runtime_embedded[] ="
  awk '
  {
    line = $0
    gsub(/\\/, "\\\\", line)
    gsub(/"/, "\\\"", line)
    printf "\"%s\\n\"\n", line
  }' "$TMP_FILE"
  echo ";"
  echo "static const unsigned int ergo_runtime_embedded_len = (unsigned int)(sizeof(ergo_runtime_embedded) - 1);"
  echo ""
  echo "#endif"
} > "$OUT_FILE"
