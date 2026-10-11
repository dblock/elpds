#!/bin/sh
set -eu
cd "$(dirname "$0")"
test -f ELPDS.EXE || { echo "Build ELPDS.EXE first." >&2; exit 1; }
python3 demo.py
dosbox-x -defaultconf -fastlaunch -c "mount d \"$PWD\"" \
  -c "imgmount c \"$PWD/demo.img\" -t hdd -size 512,63,16,65" \
  -c "d:" -c "ELPDS C:"
