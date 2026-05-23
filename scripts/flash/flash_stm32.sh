#!/usr/bin/env bash
# scripts/flash/flash_stm32.sh
# Flash STM32F407 from WSL2 via openocd
#
# Prerequisites (Windows PowerShell Admin):
#   usbipd list
#   usbipd bind   --busid <ID>
#   usbipd attach --wsl --busid <ID>
#
# Usage:
#   bash scripts/flash/flash_stm32.sh               # default build dir
#   bash scripts/flash/flash_stm32.sh build/custom   # custom build dir

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${1:-$REPO_ROOT/firmware/stm32/build}"
HEX="$BUILD_DIR/zephyr/zephyr.hex"

echo "=== SecureInferNode Flash Script ==="
echo "Build dir: $BUILD_DIR"

if [ ! -f "$HEX" ]; then
    echo "ERROR: $HEX not found"
    echo "Run first: west build -p auto -b disco_f407vg firmware/stm32"
    exit 1
fi

echo "Flashing: $HEX"
west flash --runner openocd --build-dir "$BUILD_DIR"
echo "Flash complete."
echo ""
echo "Open UART terminal:"
echo "  minicom -D /dev/ttyACM0 -b 115200"
