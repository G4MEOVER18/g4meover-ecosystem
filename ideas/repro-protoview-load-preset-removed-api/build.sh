#!/bin/sh
# Host repro build/run for the ProtoView furi_hal_subghz_load_preset()
# port finding. Run from this directory.
set -e
CFLAGS="-std=c17 -Wall -Wextra -I."
SAN="-fsanitize=address,undefined -fno-sanitize-recover=undefined"

echo "=== ALT: builds against a firmware that still has"
echo "    furi_hal_subghz_load_preset() (PROVIDE_REMOVED_SYMBOL=1) ==="
for opt in O0 O1; do
    gcc $CFLAGS -$opt $SAN -DPROTOVIEW_REPRO_PROVIDE_REMOVED_SYMBOL=1 \
        radio_begin_ALT.c modulations.c mocks.c repro_main_alt.c -o /tmp/pv_alt_ok_$opt
    echo "-$opt: build OK (old firmware) -> running"
    /tmp/pv_alt_ok_$opt
done

echo
echo "=== ALT: same code against my own firmware, i.e. WITHOUT"
echo "    furi_hal_subghz_load_preset() (PROVIDE_REMOVED_SYMBOL=0) ==="
if gcc $CFLAGS -O0 -DPROTOVIEW_REPRO_PROVIDE_REMOVED_SYMBOL=0 \
    radio_begin_ALT.c modulations.c mocks.c repro_main_alt.c -o /tmp/pv_alt_broken 2>/tmp/pv_alt_broken.log; then
    echo "UNEXPECTED: ALT built against my own firmware headers -- finding not reproduced"
    exit 1
else
    echo "EXPECTED build failure (undeclared/undefined furi_hal_subghz_load_preset):"
    grep -m1 "load_preset" /tmp/pv_alt_broken.log
fi

echo
echo "=== NEU: fixed radio_begin(), against my own firmware headers ==="
for opt in O0 O1; do
    gcc $CFLAGS -$opt $SAN -DPROTOVIEW_REPRO_PROVIDE_REMOVED_SYMBOL=0 \
        radio_begin_NEU.c modulations.c mocks.c repro_main_neu.c -o /tmp/pv_neu_$opt
    echo "-$opt: build OK (my own firmware) -> running"
    /tmp/pv_neu_$opt
done

echo
echo "ALL CHECKS PASSED (both optimization levels, both ALT-regression and NEU)"
