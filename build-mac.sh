#!/bin/sh
set -eu
cd "$(dirname "$0")"
# The SSD is exFAT: macOS leaves ._* files that break the build caches.
find . -name '._*' -not -path './build/_deps/*' -delete 2>/dev/null || true
# A build/ made when the project lived elsewhere (Desktop...) is refused by CMake: start it again.
if [ -f build/CMakeCache.txt ] && ! grep -q "CMAKE_HOME_DIRECTORY:INTERNAL=$(pwd)\$" build/CMakeCache.txt; then
  echo "build/ vient d'un autre emplacement du projet : je le recrée."
  rm -rf build
fi
# Reuse the JUCE already downloaded by another Swell project, when it is there.
JUCE_ARG=""
JUCE_DIR="$(cd .. && pwd)/Swell-Acoustic-Guitar-/build/_deps/juce-src"
[ -d "$JUCE_DIR" ] && JUCE_ARG="-DFETCHCONTENT_SOURCE_DIR_JUCE=$JUCE_DIR"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 ${JUCE_ARG:+"$JUCE_ARG"}
cmake --build build --parallel 3
ctest --test-dir build --output-on-failure
# The linker's signature does not seal the bundle; Logic and other AU hosts can refuse it.
OUT=build/SwellPercussionist_artefacts/Release
# On the exFAT SSD the bundles fill up with ._* files, which codesign refuses: remove them (and the attributes) first.
for b in "$OUT/AU/Swell The Percussionist.component" "$OUT/VST3/Swell The Percussionist.vst3" "$OUT/Standalone/Swell The Percussionist.app"; do
  find "$b" -name '._*' -delete
  xattr -cr "$b"
  codesign --force --deep -s - "$b"
  find "$b" -name '._*' -delete
  codesign --verify --deep --strict "$b"
done
