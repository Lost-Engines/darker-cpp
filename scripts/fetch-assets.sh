#!/bin/bash
# Usage: fetch-assets.sh [destination]
if [ "$#" -gt 1 ] || [ "$1" = --help ]; then
  echo "Usage: $0 [destination]"
  exit 0
fi
script_dir=$(cd -- "$(dirname -- "$0")" && pwd) || exit 1
data_home=${XDG_DATA_HOME:-$HOME/.local/share}
# The XDG base directory must be absolute.
if [[ "$data_home" != /* ]]; then data_home="$HOME/.local/share"; fi
destination=${1:-$data_home/darker}
mkdir -p -- "$destination" || exit 1
cd -- "$destination" || exit 1

# Check this file against its entry in a shared checksum manifest.
matches_checksum() {
  grep -F "  $file" "$script_dir/$1" | sha256sum --check --status 2>/dev/null
}

# 1. Fetch missing manuals and map.
while read -r url; do
  [ -z "$url" ] && continue
  file=$(basename "$url")
  if [ ! -e "$file" ]; then
    echo "Fetching $file"
    if ! curl --fail --location --retry 3 --retry-all-errors --retry-delay 2 --connect-timeout 30 --output "$file" "$url"; then
      echo "Warning: $file is unavailable. Please find a copy of the manual or map yourself."
    fi
  fi
done < "$script_dir/manual_urls.txt"

# 2. Keep valid game packs; replace missing or damaged ones.
while read -r url; do
  [ -z "$url" ] && continue
  file=$(basename "$url")
  if matches_checksum darker-retail-packs.sha256; then
    echo "$file: OK"
    continue
  fi
  echo "Fetching $file"
  if ! curl --fail --location --retry 3 --retry-all-errors --retry-delay 2 --connect-timeout 30 --output "$file" "$url"; then
    echo "Warning: $file could not be downloaded. Please supply a full game installation."
  fi
done < "$script_dir/game_urls.txt"
if ! sha256sum --check "$script_dir/darker-retail-packs.sha256"; then
  echo 'Warning: game packs are missing or do not match the known retail version. Rerun this script to retry, or supply a full game installation.'
fi

# 3. Fetch missing Roland ROMs. Other sound engines do not need these.
while read -r url; do
  [ -z "$url" ] && continue
  file=$(basename "$url")
  if [ ! -e "$file" ]; then
    echo "Fetching $file"
    if ! curl --fail --location --retry 3 --retry-all-errors --retry-delay 2 --connect-timeout 30 --output "$file" "$url"; then
      echo "Warning: $file is unavailable. Roland emulation will not be available; other sound engines will work."
    fi
  fi
done < "$script_dir/roland_rom_urls.txt"
# 4. Fetch missing SC-55 v1.21 ROMs for --music=roland-sc55.
while read -r url; do
  [ -z "$url" ] && continue
  file=$(basename "$url")
  if ! matches_checksum sc55-v121.sha256; then
    echo "Fetching $file"
    if ! curl --fail --location --retry 3 --retry-all-errors --retry-delay 2 --connect-timeout 30 --output "$file" "$url"; then
      echo "Warning: $file is unavailable. SC-55 emulation needs all five ROMs; other music options remain available."
    fi
  fi
done < "$script_dir/sc55_rom_urls.txt"
if ! sha256sum --check "$script_dir/sc55-v121.sha256"; then
  echo 'Warning: SC-55 ROMs do not match v1.21. Files have been left in place.'
fi
# 5. Fetch the AWE32 sample ROM.
while read -r url; do
  [ -z "$url" ] && continue
  file=$(basename "$url")
  if ! matches_checksum awe32.sha256; then
    echo "Fetching $file"
    if ! curl --fail --location --retry 3 --retry-all-errors --retry-delay 2 --connect-timeout 30 --output "$file" "$url"; then
      echo "Warning: AWE32 ROM unavailable; other music options remain available."
    fi
  fi
done < "$script_dir/awe32_rom_urls.txt"
if ! sha256sum --check "$script_dir/awe32.sha256"; then
  echo 'Warning: AWE32 ROM differs from the known sample ROM. File left in place.'
fi
# 6. UltraMID and its 256–1024 KiB Gravis preload patch sets.
mkdir -p ULTRASND/MIDI || exit 1
while read -r url; do
  [ -z "$url" ] && continue
  name=$(basename "$url")
  file="ULTRASND/MIDI/$name"
  if [ "$name" = ULTRAMID.EXE ]; then file="ULTRASND/$name"; fi
  if ! matches_checksum gravis.sha256; then
    echo "Fetching $file"
    if ! curl --fail --location --retry 3 --retry-all-errors --retry-delay 2 --connect-timeout 30 --output "$file" "$url"; then
      echo "Warning: $file is unavailable. Please supply the original Gravis patch set; other music options still work."
    fi
  fi
done < "$script_dir/gravis_urls.txt"
if ! sha256sum --check "$script_dir/gravis.sha256"; then
  echo 'Warning: Gravis files differ from the known patch set. Files left in place.'
fi
exit 0
