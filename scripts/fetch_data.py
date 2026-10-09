#!/usr/bin/env python3
"""Optionally fetch external game data or Roland ROMs; never overwrite local files."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request

GAME_URL = 'https://archive.org/download/darker-cdrom/Darker%20%281995%29%28Psygnosis%29.iso'
ROM_URL = ('https://archive.org/download/mame-versioned-roland-mt-32-and-cm-32l-rom-files/'
           'MT-32_and_CM-32L_MAME-Versioned_ROM_files..zip/')
GAME_HASH = '47732b89e62b6d7eaf7d03d159fa3b99352853433a90e7e92813e00659919335'
ROM_FILES = {
    'cm32l_ctrl_1_02.rom': 'a439fbb390da38cada95a7cbb1d6ca199cd66ef8',
    'cm32l_pcm.rom': '289cc298ad532b702461bfc738009d9ebe8025ea',
}


def download(url, limit):
    print(f'Downloading {url}', flush=True)
    with urllib.request.urlopen(url, timeout=60) as response:
        data = response.read(limit + 1)
    if len(data) > limit:
        raise ValueError('Download exceeds expected size')
    return data


def verify(data, expected, algorithm):
    if hashlib.new(algorithm, data).hexdigest() != expected:
        raise ValueError('Checksum mismatch; the archive may have changed. Nothing will be installed.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kind', choices=['game', 'roland'])
    parser.add_argument('destination', type=Path, help='Directory for the downloaded files')
    args = parser.parse_args()
    names = list(ROM_FILES) if args.kind == 'roland' else [f'DARKER.{i:02}' for i in range(5)]
    # Check every destination before downloading, including dangling symlinks.
    for name in names:
        path = args.destination / name
        if path.exists() or path.is_symlink():
            raise FileExistsError(f'Refusing to overwrite {path}; use your existing files or another directory')
    if args.kind == 'roland':
        files = {}
        for name, checksum in ROM_FILES.items():
            data = download(ROM_URL + name, 1048576)
            verify(data, checksum, 'sha1')  # Munt's published ROM identifiers
            files[name] = data
    else:
        extractor = shutil.which('7zz') or shutil.which('7z')
        if not extractor:
            raise RuntimeError('Game CD extraction needs 7zz or 7z (Debian/Ubuntu: apt install 7zip)')
        data = download(GAME_URL, 6252544)
        verify(data, GAME_HASH, 'sha256')
        with tempfile.TemporaryDirectory(prefix='darker-fetch-') as temporary:
            image = Path(temporary) / 'darker.iso'
            image.write_bytes(data)
            files = {name: subprocess.check_output([extractor, 'x', '-so', str(image), name]) for name in names}
        if any(not data for data in files.values()):
            raise ValueError('Missing game data in the CD image')
    # All downloads and verification complete before any destination file is created.
    args.destination.mkdir(parents=True, exist_ok=True)
    created = []
    try:
        for name, data in files.items():
            path = args.destination / name
            with path.open('xb') as output:
                created.append(path)
                output.write(data)
    except OSError:
        for path in created:
            path.unlink()
        raise
    print(f'Installed {len(files)} files in {args.destination.resolve()}')
    print('Use --mt32-rom-dir with --music=lapc1.' if args.kind == 'roland' else
          'Run Darker from this directory, or select it with --data-dir.')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, urllib.error.URLError, subprocess.CalledProcessError) as error:
        print(f'Error: {error}', file=sys.stderr)
        sys.exit(1)
