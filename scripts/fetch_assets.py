#!/usr/bin/env python3
"""Fetch optional user assets, preserving existing files and reporting individual failures."""
import argparse
from dataclasses import dataclass
import hashlib
import http.client
from pathlib import Path
import sys
import urllib.error
import urllib.request

GAME_URL = 'https://archive.org/download/darker-cdrom/Darker%20%281995%29%28Psygnosis%29.iso/'
ROM_URL = ('https://archive.org/download/mame-versioned-roland-mt-32-and-cm-32l-rom-files/'
           'MT-32_and_CM-32L_MAME-Versioned_ROM_files..zip/')


@dataclass(frozen=True)
class Asset:
    group: str
    name: str
    url: str
    limit: int
    checksum: str = ''
    algorithm: str = 'sha256'
    signature: bytes = b''


ASSETS = [
    Asset('game', f'DARKER.{index:02}', GAME_URL + f'DARKER.{index:02}', 2 * 1024 * 1024, checksum)
    for index, checksum in enumerate([
        '86bdffa2ba15edab7431c1b056feabf6dd68a70f2f0fa99ad7a96e449ad82c4f',
        '17652afc7219497704292a4234d57a3ff3c01705f7b67f811e4ea09dc1daf8a8',
        '32f32eb8d6a80e6d02b680a571abd8aa01df10772126dd6d9450120e43d1c917',
        '9982884a26927b59896b329cf1dea3c7cdd428d9a7ab70e8ac71a8754fad8ea6',
        'd72342a09edc9aa5abfb941fe27d8c81117564f15a981766ef6c5a9b8a5e6cee',
    ])
] + [
    Asset('roland', 'cm32l_ctrl_1_02.rom', ROM_URL + 'cm32l_ctrl_1_02.rom', 65536,
          'a439fbb390da38cada95a7cbb1d6ca199cd66ef8', 'sha1'),
    Asset('roland', 'cm32l_pcm.rom', ROM_URL + 'cm32l_pcm.rom', 1048576,
          '289cc298ad532b702461bfc738009d9ebe8025ea', 'sha1'),
    Asset('documents', 'Darker_Manual_DOS_EN-FR-DE-ES-IT.pdf',
          'https://d1.xp.myabandonware.com/f/m49q/Darker_Manual_DOS_EN-FR-DE-ES-IT.pdf',
          128 * 1024 * 1024, signature=b'%PDF-'),
    Asset('documents', 'Darker_Map_DOS_EN_City-Reference-Map.jpg',
          'https://d1.xp.myabandonware.com/f/m49o/Darker_Map_DOS_EN_City-Reference-Map.jpg',
          32 * 1024 * 1024, signature=b'\xff\xd8\xff'),
]


def download(asset):
    print(f'Downloading {asset.name}', flush=True)
    with urllib.request.urlopen(asset.url, timeout=60) as response:
        data = response.read(asset.limit + 1)
    if not data or len(data) > asset.limit:
        raise ValueError('Empty download or file exceeds expected size')
    if asset.checksum and hashlib.new(asset.algorithm, data).hexdigest() != asset.checksum:
        raise ValueError('Checksum mismatch; source may have changed')
    if asset.signature and not data.startswith(asset.signature):
        raise ValueError('Response is not the expected document format')
    return data


def fetch_game(assets, destination):
    paths = [destination / asset.name for asset in assets]
    existing = [path.exists() or path.is_symlink() for path in paths]
    if all(path.is_file() and not path.is_symlink() for path in paths):
        print('Keeping the existing complete game pack set.')
        return 0, len(assets)
    if any(existing):
        raise ValueError('Partial or conflicting game pack set already exists; no game files changed')
    # Stage the entire verified set in memory before creating any game files.
    staged = []
    for asset, path in zip(assets, paths):
        try:
            staged.append((path, download(asset)))
        except (OSError, ValueError, urllib.error.URLError, http.client.HTTPException) as error:
            raise ValueError(f'{asset.name}: {error}') from error
    destination.mkdir(parents=True, exist_ok=True)
    created = []
    try:
        for path, data in staged:
            with path.open('xb') as output:
                created.append(path)
                output.write(data)
    except OSError:
        for path in created:
            path.unlink()
        raise
    return len(assets), 0


def fetch(assets, destination):
    failures = []
    downloaded = skipped = 0
    game = [asset for asset in assets if asset.group == 'game']
    if game:
        try:
            downloaded, skipped = fetch_game(game, destination)
        except (OSError, ValueError, urllib.error.URLError, http.client.HTTPException) as error:
            failures.append('Complete game installation (DARKER.00 through DARKER.04)')
            print(f'Game download failed: {error}. Supply a full game installation; do not mix pack versions.',
                  file=sys.stderr, flush=True)
    for asset in assets:
        if asset.group == 'game':
            continue
        path = destination / asset.name
        try:
            if path.is_file() and not path.is_symlink():
                print(f'Keeping existing {path}')
                skipped += 1
                continue
            if path.exists() or path.is_symlink():
                raise FileExistsError(f'Destination is not a regular file: {path}')
            data = download(asset)
            destination.mkdir(parents=True, exist_ok=True)
            # Exclusive creation protects local files even if created during the download.
            with path.open('xb') as output:
                try:
                    output.write(data)
                    output.flush()
                except OSError:
                    output.close()
                    path.unlink()
                    raise
            downloaded += 1
        except (OSError, ValueError, urllib.error.URLError, http.client.HTTPException) as error:
            failures.append(asset.name)
            print(f'Failed {asset.name}: {error}', file=sys.stderr, flush=True)
    print(f'{downloaded} downloaded, {skipped} existing, {len(failures)} failed.')
    if failures:
        print(f'Find your own copies of these files and place them in {destination.resolve()}:', file=sys.stderr)
        for name in failures:
            print(f'  {name}', file=sys.stderr)
    return 1 if failures else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path, nargs='?', default=Path('.'),
                        help='Asset directory (default: current directory)')
    parser.add_argument('--only', nargs='+', choices=['game', 'roland', 'documents'],
                        default=['game', 'roland', 'documents'], help='Fetch only the selected groups')
    args = parser.parse_args()
    return fetch([asset for asset in ASSETS if asset.group in args.only], args.destination)


if __name__ == '__main__':
    sys.exit(main())
