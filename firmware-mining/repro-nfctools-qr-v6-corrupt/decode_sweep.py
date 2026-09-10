#!/usr/bin/env python3
"""Round-trip sweep for qr_repro_harness: encode random text of every length
1..140 with the built harness binary (ALT or NEU), render it, decode the
rendered bitmap with a standard QR reader (pyzbar/zbar), and compare against
the original text. Requires: pip install pyzbar pillow (+ libzbar0 on the
system). Usage: python3 decode_sweep.py ./gen_ppm_old   (or ./gen_ppm_new)
"""
import subprocess
import string
import random
import sys

from pyzbar.pyzbar import decode
from PIL import Image

if len(sys.argv) != 2:
    print(f"usage: {sys.argv[0]} <harness-binary>")
    sys.exit(2)

binary = sys.argv[1]
random.seed(42)
fail = 0
tested = 0

for n in list(range(1, 135)) + [135, 136, 140]:
    text = "".join(random.choice(string.ascii_letters + string.digits + "!?./:") for _ in range(n))
    r = subprocess.run([binary, text, "out.pgm"], capture_output=True, text=True)
    tested += 1
    if n > 134:
        # Above the true single-block-or-corrected-two-block v1-v6 capacity:
        # must be rejected ("QR unavailable" in the app), never a corrupt code.
        if "encode failed" not in r.stderr:
            print(f"n={n}: expected encode failure, got: {r.stderr.strip()}")
            fail += 1
        continue
    if r.returncode != 0:
        print(f"n={n}: nonzero exit / sanitizer issue: {r.stderr}")
        fail += 1
        continue
    res = decode(Image.open("out.pgm"))
    ok = bool(res) and res[0].data.decode() == text
    if not ok:
        print(f"n={n}: DECODE MISMATCH info={r.stderr.strip()!r} got={res}")
        fail += 1

print(f"tested={tested} fail={fail}")
sys.exit(1 if fail else 0)
