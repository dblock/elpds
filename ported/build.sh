#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${WATCOM:?Set WATCOM to an extracted Open Watcom installation containing binw, h, and lib286}"
rm -f ELPDS.EXE BUILD.LOG BUILD.OK
dosbox-x -silent -defaultconf -c "mount w \"$WATCOM\"" \
  -c "mount d \"$PWD\"" -c "set WATCOM=W:\\" \
  -c "set PATH=W:\\BINW" -c "set INCLUDE=W:\\H" \
  -c "d:" -c "BUILD.BAT" -c "exit"
cat BUILD.LOG
test -f ELPDS.EXE
test -f BUILD.OK
echo "Built ported/ELPDS.EXE"
