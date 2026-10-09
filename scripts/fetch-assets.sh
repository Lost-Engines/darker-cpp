#!/bin/bash
# Usage: fetch-assets.sh [destination]; existing files are kept.
if [ "$#" -gt 1 ] || [ "${1:-}" = --help ]; then
  echo "Usage: $0 [destination]"
  exit 0
fi
mkdir -p -- "${1:-.}" && cd -- "${1:-.}" || exit 1
stage=$(mktemp -d .fetch-assets.XXXXXX) || exit 1
trap 'rm -rf -- "$stage"' EXIT
trap 'exit 1' INT TERM
failed=0

game_url='https://archive.org/download/darker-cdrom/Darker%20%281995%29%28Psygnosis%29.iso'
rom_url='https://archive.org/download/mame-versioned-roland-mt-32-and-cm-32l-rom-files/MT-32_and_CM-32L_MAME-Versioned_ROM_files..zip'
packs=(DARKER.0{0..4})

download() {
  echo "Fetching $2"
  curl -fsSL --connect-timeout 15 --max-time 180 "$1" -o "$stage/$2"
}

fetch_game() {
  local present=0 file
  local created=()
  for file in "${packs[@]}"; do
    if [ -e "$file" ] || [ -L "$file" ]; then present=$((present + 1)); fi
  done
  if [ "$present" -eq 5 ]; then echo 'Keeping existing game packs'; return 0; fi
  if [ "$present" -ne 0 ]; then echo 'Partial local game installation; leaving it untouched'; return 1; fi
  for file in "${packs[@]}"; do download "$game_url/$file" "$file" || return 1; done
  (cd "$stage" && sha256sum -c <<'SUMS'
86bdffa2ba15edab7431c1b056feabf6dd68a70f2f0fa99ad7a96e449ad82c4f  DARKER.00
17652afc7219497704292a4234d57a3ff3c01705f7b67f811e4ea09dc1daf8a8  DARKER.01
32f32eb8d6a80e6d02b680a571abd8aa01df10772126dd6d9450120e43d1c917  DARKER.02
9982884a26927b59896b329cf1dea3c7cdd428d9a7ab70e8ac71a8754fad8ea6  DARKER.03
d72342a09edc9aa5abfb941fe27d8c81117564f15a981766ef6c5a9b8a5e6cee  DARKER.04
SUMS
  ) || return 1
  for file in "${packs[@]}"; do
    # Link within the destination filesystem: never overwrite a local file.
    if ! ln -T -- "$stage/$file" "$file"; then
      if [ "${#created[@]}" -gt 0 ]; then rm -f -- "${created[@]}"; fi
      return 1
    fi
    created+=("$file")
  done
}

if ! fetch_game; then
  rm -f -- "$stage"/DARKER.0{0..4}
  echo 'Game download failed. Please supply a full game installation (DARKER.00–04).' >&2
  failed=1
fi

for url in \
  "$rom_url/cm32l_ctrl_1_02.rom" \
  "$rom_url/cm32l_pcm.rom" \
  'https://d1.xp.myabandonware.com/f/m49q/Darker_Manual_DOS_EN-FR-DE-ES-IT.pdf' \
  'https://d1.xp.myabandonware.com/f/m49o/Darker_Map_DOS_EN_City-Reference-Map.jpg'; do
  file=${url##*/}
  if [ -e "$file" ] || [ -L "$file" ]; then echo "Keeping $file"; continue; fi
  if ! download "$url" "$file" || ! ln -T -- "$stage/$file" "$file"; then
    echo "Failed: $file — please obtain a copy yourself." >&2
    failed=1
  fi
done
exit "$failed"
