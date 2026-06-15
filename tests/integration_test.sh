#!/usr/bin/env bash
# Integration test: drive the real ncurses binary end-to-end inside a
# pseudo-terminal. Verifies that startup, color initialization, at least one
# render frame, and clean 'q' shutdown all work together -- in both the
# colored and the NO_COLOR monochrome modes.
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

# ncurses needs a usable terminal type even under script(1).
export TERM="${TERM:-xterm}"

make >/dev/null 2>&1 || { echo "FAIL: build failed"; exit 1; }

run_once() {
    # $1 = label, rest of env passed by caller
    local label="$1"; shift
    local ts; ts="$(mktemp)"
    # Feed 'q' on stdin to request a clean quit; timeout is only a safety net.
    printf 'q' | timeout 10 script -qec './ascii-monitor' "$ts" >/dev/null 2>&1
    local rc=$?
    if [ "$rc" -ne 0 ]; then
        echo "FAIL [$label]: did not exit cleanly (rc=$rc)"
        rm -f "$ts"; return 1
    fi
    if ! grep -q "quit" "$ts"; then
        echo "FAIL [$label]: expected UI text ('quit') not rendered"
        rm -f "$ts"; return 1
    fi
    printf '%s\n' "$ts"
    return 0
}

# --- colored run -------------------------------------------------------------
ts1="$(run_once color)" || exit 1
echo "PASS [color]: started, rendered, and quit cleanly"

# --- monochrome fallback (NO_COLOR convention) -------------------------------
ts2="$(mktemp)"
printf 'q' | NO_COLOR=1 timeout 10 script -qec './ascii-monitor' "$ts2" >/dev/null 2>&1
rc=$?
if [ "$rc" -ne 0 ]; then echo "FAIL [mono]: did not exit cleanly (rc=$rc)"; exit 1; fi
if ! grep -q "mono" "$ts2"; then echo "FAIL [mono]: monochrome marker not shown"; exit 1; fi
echo "PASS [mono]: NO_COLOR monochrome fallback works"

rm -f "$ts1" "$ts2"
echo "[integration] PASSED"
