#!/bin/bash
# Usage: fetch-assets.sh [destination]; existing files are kept.
if [ "$#" -gt 1 ] || [ "${1:-}" = --help ]; then
  echo "Usage: $0 [destination]"
  exit 0
fi
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd) || exit 1
checksums="$script_dir/darker-retail-packs.sha256"
mkdir -p -- "${1:-.}" && cd -- "${1:-.}" || exit 1
stage=$(mktemp -d .fetch-assets.XXXXXX) || exit 1
trap 'rm -rf -- "$stage"' EXIT
trap 'exit 1' INT TERM
failed=0

packs=(DARKER.0{0..4})

download() {
  echo "Fetching $2"
  curl -fsSL --connect-timeout 15 --max-time 180 "$1" -o "$stage/$2"
}

fetch_game() {
  local present=0 file url
  local created=()
  for file in "${packs[@]}"; do
    if [ -e "$file" ] || [ -L "$file" ]; then present=$((present + 1)); fi
  done
  if [ "$present" -eq 5 ]; then echo 'Keeping existing game packs'; return 0; fi
  if [ "$present" -ne 0 ]; then echo 'Partial local game installation; leaving it untouched'; return 1; fi
  if [ ! -r "$checksums" ] || [ ! -r "$script_dir/game_urls.txt" ]; then
    echo 'Keep game_urls.txt and darker-retail-packs.sha256 beside this script.' >&2
    return 1
  fi
  while IFS= read -r url || [ -n "$url" ]; do
    [ -z "$url" ] && continue
    download "$url" "${url##*/}" || return 1
  done < "$script_dir/game_urls.txt" || return 1
  (cd "$stage" && sha256sum --check "$checksums") || return 1
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

for list in roland_rom_urls.txt manual_urls.txt; do
  if [ ! -r "$script_dir/$list" ]; then
    echo "Missing $list. Please keep it beside this script." >&2
    failed=1
    continue
  fi
  while IFS= read -r url || [ -n "$url" ]; do
    [ -z "$url" ] && continue
    file=${url##*/}
    if [ -e "$file" ] || [ -L "$file" ]; then echo "Keeping $file"; continue; fi
    if ! download "$url" "$file" || ! ln -T -- "$stage/$file" "$file"; then
      echo "Failed: $file — please obtain a copy yourself." >&2
      failed=1
    fi
  done < "$script_dir/$list"
done
exit "$failed"
