#!/usr/bin/env sh
set -eu

ERGO_BIN="${1:-./ergo/build/ergo}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

run_capture() {
    name="$1"
    shift
    out="$TMP/$name.out"
    set +e
    "$@" >"$out" 2>&1
    rc=$?
    set -e
    echo "$rc" >"$TMP/$name.rc"
}

run_capture missing_warn "$ERGO_BIN" lint ergo/tests/lint/missing_return.ergo
run_capture missing_strict "$ERGO_BIN" lint --mode strict ergo/tests/lint/missing_return.ergo
run_capture safe_warn "$ERGO_BIN" lint ergo/tests/lint/safe_return.ergo
run_capture index_warn "$ERGO_BIN" lint ergo/tests/lint/index_to_nonnull_assignment.ergo
run_capture index_coalesce "$ERGO_BIN" lint ergo/tests/lint/index_with_coalesce_ok.ergo
run_capture truthiness "$ERGO_BIN" lint ergo/tests/lint/truthiness_warning.ergo

[ "$(cat "$TMP/missing_warn.rc")" = "0" ]
[ "$(cat "$TMP/missing_strict.rc")" = "1" ]
[ "$(cat "$TMP/safe_warn.rc")" = "0" ]
[ "$(cat "$TMP/index_warn.rc")" = "0" ]
[ "$(cat "$TMP/index_coalesce.rc")" = "0" ]
[ "$(cat "$TMP/truthiness.rc")" = "0" ]

grep -q "missing return coverage in function 'choose'" "$TMP/missing_warn.out"
grep -q "error: .*missing return coverage in function 'choose'" "$TMP/missing_strict.out"
! grep -q "missing return coverage" "$TMP/safe_warn.out"
grep -q "indexing expression may yield null when used as a non-null assignment" "$TMP/index_warn.out"
! grep -q "indexing expression may yield null" "$TMP/index_coalesce.out"
grep -q "implicit truthiness in if condition" "$TMP/truthiness.out"

echo "lint tests passed"
