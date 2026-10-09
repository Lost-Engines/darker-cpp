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

# 1. Fetch missing manuals and map.
while read -r url; do
  [ -z "$url" ] && continue
  file=$(basename "$url")
  if [ ! -e "$file" ]; then
    echo "Fetching $file"
    if ! curl --fail --location --output "$file" "$url"; then
      echo "Warning: $file is unavailable. Please find a copy of the manual or map yourself."
    fi
  fi
done < "$script_dir/manual_urls.txt"

# 2. DARKER.00 indicates an existing installation; otherwise replace all packs.
if [ -e DARKER.00 ]; then
  echo 'DARKER.00 exists; skipping game downloads.'
else
  while read -r url; do
    [ -z "$url" ] && continue
    file=$(basename "$url")
    echo "Fetching $file"
    if ! curl --fail --location --output "$file" "$url"; then
      echo "Warning: $file could not be downloaded. Please supply a full game installation."
    fi
  done < "$script_dir/game_urls.txt"
  if ! sha256sum --check "$script_dir/darker-retail-packs.sha256"; then
    echo 'Warning: game packs do not match the known retail version. Files have been left in place.'
  fi
fi

# 3. Fetch missing Roland ROMs. Other sound engines do not need these.
while read -r url; do
  [ -z "$url" ] && continue
  file=$(basename "$url")
  if [ ! -e "$file" ]; then
    echo "Fetching $file"
    if ! curl --fail --location --output "$file" "$url"; then
      echo "Warning: $file is unavailable. Roland emulation will not be available; other sound engines will work."
    fi
  fi
done < "$script_dir/roland_rom_urls.txt"
# 4. Fetch missing SC-55 v1.21 ROMs for --music=roland-sc55.
while read -r url; do
  [ -z "$url" ] && continue
  file=$(basename "$url")
  if [ ! -e "$file" ]; then
    echo "Fetching $file"
    if ! curl --fail --location --output "$file" "$url"; then
      echo "Warning: $file is unavailable. SC-55 emulation needs all five ROMs; other music options remain available."
    fi
  fi
done < "$script_dir/sc55_rom_urls.txt"
if ! sha256sum --check "$script_dir/sc55-v121.sha256"; then
  echo 'Warning: SC-55 ROMs do not match v1.21. Files have been left in place.'
fi
exit 0
