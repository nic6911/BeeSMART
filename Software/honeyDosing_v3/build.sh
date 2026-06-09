#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SKETCH="honeyDosing_v3.ino"
DATA_DIR="data"
BUILD_DIR="build"
FQBN="esp32:esp32:esp32c3"
FLASH_SIZE="4MB"
FLASH_MODE="dio"
FLASH_FREQ="80m"

COMBINED_BIN="$BUILD_DIR/honeyDosing_v3_combined.bin"

echo "=== 1/3: Compiling $SKETCH for $FQBN ==="
arduino-cli compile --fqbn "$FQBN" "$SKETCH" --build-path "$BUILD_DIR"

echo ""
echo "=== 2/3: Building LittleFS image from $DATA_DIR/ ==="
python3 /dev/stdin <<PYEOF
import os, struct
from littlefs import LittleFS

data_dir = "$DATA_DIR"
block_size = 4096
block_count = 352

lfs = LittleFS(
    block_size=block_size,
    block_count=block_count,
    read_size=256,
    prog_size=256,
)

for root, dirs, files in os.walk(data_dir):
    for fname in files:
        rel_dir = os.path.relpath(root, data_dir)
        lpath = os.path.join("/", rel_dir, fname) if rel_dir != "." else "/" + fname
        with open(os.path.join(root, fname), "rb") as f:
            content = f.read()
        lfs.makedirs(os.path.dirname(lpath))
        with lfs.open(lpath, "wb") as f:
            f.write(content)
        print(f"  Added: {lpath} ({len(content)} bytes)")

with open("$BUILD_DIR/littlefs.bin", "wb") as f:
    f.write(lfs.context.buffer)

fsize = os.path.getsize("$BUILD_DIR/littlefs.bin")
expect = block_size * block_count
assert fsize == expect, f"Size mismatch: {fsize} != {expect}"
print(f"  LittleFS image: {fsize} bytes ({fsize/1024/1024:.2f} MB)")
PYEOF

echo ""
echo "=== 3/3: Merging into single binary ==="
BOOTLOADER="$BUILD_DIR/$SKETCH.bootloader.bin"
PARTITIONS="$BUILD_DIR/$SKETCH.partitions.bin"
APP="$BUILD_DIR/$SKETCH.bin"
LITTLEFS="$BUILD_DIR/littlefs.bin"

python3 -m esptool --chip esp32c3 merge-bin \
    --flash-mode "$FLASH_MODE" \
    --flash-freq "$FLASH_FREQ" \
    --flash-size "$FLASH_SIZE" \
    --output "$COMBINED_BIN" \
    0x00000 "$BOOTLOADER" \
    0x08000 "$PARTITIONS" \
    0x10000 "$APP" \
    0x290000 "$LITTLEFS"

echo ""
echo "=== Done! ==="
echo "Combined binary: $SCRIPT_DIR/$COMBINED_BIN"
ls -lh "$COMBINED_BIN"

echo ""
echo "Flash it with:"
echo "  python3 -m esptool --chip esp32c3 -p /dev/ttyACM0 write-flash 0x0 $SCRIPT_DIR/$COMBINED_BIN"
