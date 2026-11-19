#!/bin/sh
# Builds and runs repro.c at -O0 and -O1 under ASan/UBSan, as documented in
# ideas/protoview-ford-tpms-decoder-length-check-fix.md.
set -e
cd "$(dirname "$0")"
for opt in O0 O1; do
    echo "=== -$opt ==="
    gcc -std=c17 -Wall -Wextra -$opt -fsanitize=address,undefined \
        -fno-sanitize-recover=undefined -o "repro_$opt" repro.c
    ./"repro_$opt"
done
