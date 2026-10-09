#!/bin/bash
# Usage: fetch-assets.sh [destination]
if [ "$#" -gt 1 ] || [ "$1" = --help ]; then
  echo "Usage: $0 [destination]"
  exit 0
fi
script_dir=$(cd -- "$(dirname -- "$0")" && pwd) || exit 1
destination=${1:-.}
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
exit 0
