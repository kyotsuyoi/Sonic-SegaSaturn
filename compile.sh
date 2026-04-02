#!/bin/bash
set -e

BUILD_DIR="game_build"
TRACKS_DIR="game_tracks"

./clean.sh
mkdir -p "${BUILD_DIR}"

export NCPU=`nproc`
make -j${NCPU} all

for f in game.bin game.cue game_bin.cue game.elf game.iso game.map game.raw; do
		if [ -f "$f" ]; then
				mv -f "$f" "${BUILD_DIR}/$f"
		fi
done

cp -f "${TRACKS_DIR}"/game\ \(track\ *\).wav "${BUILD_DIR}"/ 2>/dev/null || true

if [ -f "${BUILD_DIR}/game.iso" ]; then
		cp -f "${BUILD_DIR}/game.iso" "${BUILD_DIR}/game.bin"
fi

if [ -f "${BUILD_DIR}/game.iso" ]; then
cat > "${BUILD_DIR}/game.cue" << 'EOF'
FILE "game.iso" BINARY
	TRACK 01 MODE1/2048
			INDEX 01 00:00:00
			POSTGAP 00:02:00
FILE "game (track 2).wav" WAVE
	TRACK 02 AUDIO
		PREGAP 00:02:00
		INDEX 01 00:00:00
FILE "game (track 3).wav" WAVE
	TRACK 03 AUDIO
		INDEX 01 00:00:00
FILE "game (track 4).wav" WAVE
	TRACK 04 AUDIO
		INDEX 01 00:00:00
EOF

cat > "${BUILD_DIR}/game_bin.cue" << 'EOF'
FILE "game.bin" BINARY
	TRACK 01 MODE1/2048
			INDEX 01 00:00:00
	POSTGAP 00:02:00
FILE "game (track 2).wav" WAVE
	TRACK 02 AUDIO
		PREGAP 00:02:00
		INDEX 01 00:00:00
FILE "game (track 3).wav" WAVE
	TRACK 03 AUDIO
		INDEX 01 00:00:00
FILE "game (track 4).wav" WAVE
	TRACK 04 AUDIO
		INDEX 01 00:00:00
EOF
fi

exit 0

