#!/bin/sh
# Builds and runs the Oregon2 decoder host-repro:
#  1) gcc + ASan/UBSan at -O0 and -O1: functional-correctness proof for
#     the temp[]/hum[] ASCII-offset bug (deterministic, no sanitizer
#     needed to see it -- it is a pure arithmetic error).
#  2) gcc (no sanitizer) + Valgrind Memcheck: proof that ALT reads
#     deviceid[] before it is ever written, and that NEU does not.
set -e
cd "$(dirname "$0")"

echo "=== gcc -O0 -fsanitize=address,undefined ==="
gcc -std=c11 -Wall -Wextra -O0 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -o repro_O0 mocks.c oregon2_extract_ALT.c oregon2_extract_NEU.c repro_main.c -lm
./repro_O0

echo
echo "=== gcc -O1 -fsanitize=address,undefined ==="
gcc -std=c11 -Wall -Wextra -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -o repro_O1 mocks.c oregon2_extract_ALT.c oregon2_extract_NEU.c repro_main.c -lm
./repro_O1

echo
echo "=== gcc -O0 (no sanitizer) + Valgrind Memcheck: uninitialized deviceid[] ==="
gcc -std=c11 -Wall -Wextra -O0 -g \
    -o uninit_run mocks.c oregon2_extract_ALT.c oregon2_extract_NEU.c uninit_main.c
set +e
valgrind --error-exitcode=99 --track-origins=yes --log-file=valgrind.log ./uninit_run
rc=$?
set -e
cat valgrind.log

if [ "$rc" -ne 99 ]; then
    echo "UNEXPECTED: exit code $rc (expected 99, i.e. Valgrind found errors)."
    exit 1
fi
if ! grep -q "oregon2_extract_ALT (oregon2_extract_ALT.c:14)" valgrind.log; then
    echo "UNEXPECTED: no 'Uninitialised value was created by a stack allocation' pointing at oregon2_extract_ALT.c:14 (the 'uint8_t deviceid[2];' declaration)."
    exit 1
fi
echo "Memcheck confirmed: deviceid[] (oregon2_extract_ALT.c:14) is read while uninitialized."

echo
echo "=== Same binary, NEU-only run must be Valgrind-clean ==="
gcc -std=c11 -Wall -Wextra -O0 -g \
    -o uninit_run_neu_only mocks.c oregon2_extract_NEU.c uninit_main_neu_only.c
valgrind --error-exitcode=99 ./uninit_run_neu_only
echo "NEU-only run: Valgrind-clean, as expected."

echo
echo "ALL CHECKS PASSED"
