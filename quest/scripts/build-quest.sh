#!/usr/bin/env bash
# Builds SaucyMenu.qmod for the standalone Quest. Run inside WSL (Ubuntu) from the repo root:
#
#   bash quest/scripts/build-quest.sh /mnt/c/Users/<you>/Documents/files
#
# It installs build tools if missing, installs qpm (Quest Package Manager) into ~/.local/bin, downloads
# the pinned dependencies (scotland2, beatsaber-hook, flamingo, paperlog) and the Android NDK through qpm, builds,
# and copies SaucyMenu.qmod plus saucy-build.log into the folder you pass. It never touches your headset.
set -euo pipefail

OUT="${1:?Pass the Windows folder to copy the result to, e.g. /mnt/c/Users/<you>/Documents/files}"
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOG="$OUT/saucy-build.log"
mkdir -p "$OUT"
exec > >(tee "$LOG") 2>&1

echo "== Saucy Menu Quest build $(date -Is)"
echo "repo: $REPO"
git -C "$REPO" log -1 --format='commit: %h %s' || true

# 1. Build tools
missing=()
for tool in cmake ninja unzip curl git; do command -v "$tool" >/dev/null || missing+=("$tool"); done
if ((${#missing[@]})); then
  echo "== Installing: ${missing[*]}"
  pkgs=("${missing[@]/ninja/ninja-build}")
  sudo apt-get update && sudo apt-get install -y "${pkgs[@]}"
fi

# 2. qpm
export PATH="$HOME/.local/bin:$PATH"
if ! command -v qpm >/dev/null; then
  echo "== Installing qpm"
  mkdir -p "$HOME/.local/bin"
  tmp="$(mktemp -d)"
  curl -sSL -o "$tmp/qpm.zip" https://github.com/QuestPackageManager/QPM.CLI/releases/latest/download/qpm-linux-x64.zip
  unzip -o -q "$tmp/qpm.zip" -d "$HOME/.local/bin"
  chmod +x "$HOME/.local/bin/qpm"
fi
qpm --version

# 3. Dependencies + NDK, then build
cd "$REPO/quest/mod"
echo "== qpm ndk resolve"
qpm ndk resolve -d
echo "== qpm restore"
qpm restore
echo "== build"
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -B build .
cmake --build build
echo "== package"
qpm qmod zip --skip_build SaucyMenu.qmod

cp SaucyMenu.qmod "$OUT/"
echo "== Done: $OUT/SaucyMenu.qmod"
