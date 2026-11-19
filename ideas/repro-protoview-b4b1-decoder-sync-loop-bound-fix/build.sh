#!/bin/sh
# Builds and runs the B4B1 sync-search loop-bound repro at -O0 and -O1,
# with ASan+UBSan, and fails (non-zero exit) on any check failure, compiler
# warning, or sanitizer report.
set -e
cd "$(dirname "$0")"

for opt in -O0 -O1; do
    echo "=== building at $opt ==="
    gcc -std=c17 -Wall -Wextra "$opt" -fsanitize=address,undefined -fno-sanitize-recover=undefined \
        -o repro_main \
        mocks.c b4b1_seek_sync_ALT.c b4b1_seek_sync_NEU.c repro_main.c
    echo "=== running at $opt ==="
    ./repro_main
    echo
done

rm -f repro_main
echo "build.sh: all builds and runs completed"
