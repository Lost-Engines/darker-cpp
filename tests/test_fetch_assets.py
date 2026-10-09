"""Asset integrity and all-or-nothing game installation checks (no network)."""
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import fetch_assets as fetcher


class FetchAssetsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.destination = Path(self.temporary.name)
        self.game = [a for a in fetcher.ASSETS if a.group == 'game']
        self.rom = next(a for a in fetcher.ASSETS if a.group == 'roland')

    def run_fetch(self, assets):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return fetcher.fetch(assets, self.destination)

    def test_failed_pack_leaves_no_game_files_and_continues_optional_assets(self):
        def download(asset):
            if asset == self.game[2]:
                raise OSError('Connection failed')
            return b'verified data'
        with patch.object(fetcher, 'download', side_effect=download):
            self.assertEqual(self.run_fetch(self.game + [self.rom]), 1)
        self.assertEqual(sorted(p.name for p in self.destination.iterdir()), [self.rom.name])

    def test_partial_local_installation_is_preserved_without_download(self):
        existing = self.destination / self.game[0].name
        existing.write_bytes(b'local copy')
        with patch.object(fetcher, 'download') as download:
            self.assertEqual(self.run_fetch(self.game), 1)
            download.assert_not_called()
        self.assertEqual(existing.read_bytes(), b'local copy')

    def test_complete_local_installation_is_preserved(self):
        for asset in self.game:
            (self.destination / asset.name).write_bytes(b'local copy')
        with patch.object(fetcher, 'download') as download:
            self.assertEqual(self.run_fetch(self.game), 0)
            download.assert_not_called()

    def test_write_failure_rolls_back_new_game_files(self):
        original_open = Path.open
        def open_file(path, *args, **kwargs):
            if path.name == self.game[2].name and args == ('xb',):
                raise OSError('Disk full')
            return original_open(path, *args, **kwargs)
        with patch.object(fetcher, 'download', return_value=b'verified data'), patch.object(Path, 'open', open_file):
            self.assertEqual(self.run_fetch(self.game), 1)
        self.assertEqual(list(self.destination.iterdir()), [])

    def test_corrupt_pack_is_rejected(self):
        with patch.object(fetcher.urllib.request, 'urlopen', return_value=io.BytesIO(b'wrong version')):
            with self.assertRaisesRegex(ValueError, 'Checksum mismatch'):
                fetcher.download(self.game[0])

    def test_manifest_matches_downloader(self):
        manifest = Path(__file__).resolve().parents[1] / 'docs/retail-pack-checksums.json'
        for entry, asset in zip(json.loads(manifest.read_text())['packs'], self.game, strict=True):
            self.assertEqual(entry['file'], asset.name)
            self.assertEqual(entry['sha256'], asset.checksum)


if __name__ == '__main__':
    unittest.main()
