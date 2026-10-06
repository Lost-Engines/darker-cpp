#!/usr/bin/env python3
"""Compare C++ cockpit output with independently extracted masked HUD previews.

Development-only; requires Pillow and the parent project's analysis exports.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--viewer', type=Path, default=Path(__file__).resolve().parents[1] / 'build/darker')
    parser.add_argument('--workspace', type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    root = args.workspace.resolve()
    hud = root / 'analysis/interface/hud'
    definitions = json.loads((hud / 'hud.json').read_text())['definitions']
    cases = 0
    with tempfile.TemporaryDirectory(prefix='darker-cockpit-') as temporary:
        output = Path(temporary) / 'actual.ppm'

        def check(craft, expected, *options):
            nonlocal cases
            subprocess.run([str(args.viewer.resolve()), '--data-dir', str(root / 'darker'), '--craft', craft,
                            '--fill', '0', '--output', str(output), *map(str, options)], check=True)
            with Image.open(output) as image:
                actual = image.convert('RGB')
            assert actual.size == expected.size and actual.tobytes() == expected.convert('RGB').tobytes(), (craft, options)
            cases += 1

        for craft, resource in [('caero', 16), ('skimma', 17), ('upgraded', 18)]:
            background = Image.open(root / f'analysis/interface/cockpit-{resource}-static.png').convert('RGBA')
            check(craft, background, '--static')
            background.paste((0, 0, 0, 255), (0, 8 if resource == 16 else 0, 320, 176 if resource == 16 else 180))
            check(craft, background)
            components = sorted((d for d in definitions if resource in d['resources']), key=lambda d: d['field'])
            for field, descriptor in enumerate(components):
                limit = 16 if resource == 17 and descriptor['field'] == 0x454d else len(descriptor['records'])
                expected = background.copy()
                for count, record in enumerate(descriptor['records'][:limit], 1):
                    strip = Image.open(hud / record['previews'][str(resource)]['on']).convert('RGBA')
                    expected.alpha_composite(strip, tuple(record['destination']))
                    check(craft, expected, '--field', field, '--states', count)
                    # Some receiver masks overlap: preserve history instead of assuming a canonical count image.
                    decreased = background.copy()
                    for active in descriptor['records'][:limit]:
                        decreased.alpha_composite(Image.open(hud / active['previews'][str(resource)]['on']).convert('RGBA'), tuple(active['destination']))
                    for restored in descriptor['records'][count:limit]:
                        decreased.alpha_composite(Image.open(hud / restored['previews'][str(resource)]['base']).convert('RGBA'), tuple(restored['destination']))
                    check(craft, decreased, '--field', field, '--states', limit, count)
                check(craft, background, '--field', field, '--states', limit, 0)
                if descriptor['field'] == 0x4552 and resource == 16:
                    record = descriptor['records'][0]
                    dim = background.copy()
                    dim.alpha_composite(Image.open(hud / record['previews'][str(resource)]['alternate']).convert('RGBA'), tuple(record['destination']))
                    check(craft, dim, '--field', field, '--states', 1, 129)
                    check(craft, expected, '--field', field, '--states', 1, 129, 1)
    print(f'{cases} cockpit comparisons passed: static caches, all supported strip counts, decreases/restoration and engine dimming.')


if __name__ == '__main__':
    main()
