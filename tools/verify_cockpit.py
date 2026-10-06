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
    sprites_dir = root / 'analysis/interface/sprites'
    sprites_data = json.loads((sprites_dir / 'sprites.json').read_text())
    compass = json.loads((sprites_dir / 'procedural-hud.json').read_text())['compass']
    procedural = json.loads((root / 'analysis/interface/dynamic/procedural.json').read_text())
    radar = json.loads((root / 'analysis/interface/dynamic/verification.json').read_text())['radar_checks']
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
            if resource == 16:
                sprites = root / 'analysis/interface/sprites'
                for x in (44, 56):
                    for digit, offset in ((0, 0), (1, 4)):
                        background.alpha_composite(Image.open(sprites / f'16-digit-small-{digit:02}.png').convert('RGBA'), (x + offset, 185))
                for selection, x in ((1, 260), (5, 268)):
                    background.alpha_composite(Image.open(sprites / f'16-weapon-{selection:02}.png').convert('RGBA'), (x, 195))
                for point in compass['phases'][0]['erase'] + compass['phases'][34]['draw']:
                    background.putpixel((point['x'], point['y']), tuple(point['rgb']) + (255,))
                palette = sprites_data['palettes']['16']
                for x, y, kind in [(-10, 10, 0), (5, -5, 1)]:
                    sample = next(s for s in radar if (s['heading'], s['x'], s['y'], s['kind']) == (0, x, y, kind))
                    for px, py, colour in sample['points']:
                        background.putpixel((px, py), tuple(palette[colour*3:colour*3+3]) + (255,))
            else:
                identifiers = [f'{resource}-callback-5227-0-0', f'{resource}-callback-52e0-0-1', f'{resource}-callback-52e0-1-2']
                if resource == 18:
                    identifiers.append(f'{resource}-callback-52e0-2-3')
                for identifier in identifiers:
                    asset = next(a for a in sprites_data['assets'] if a['id'] == identifier)
                    background.alpha_composite(Image.open(sprites_dir / asset['file']).convert('RGBA'), tuple(asset['destination']))
                ring = next(r for r in procedural['verification']['ring_cases'] if (r['weapon'], r['radius'], r['count']) == (0, 63, 14))
                for blit in ring['blits']:
                    asset = next(a for a in procedural['ring']['assets'] if a['resource'] == resource and a['source'][:2] == blit['source'])
                    background.alpha_composite(Image.open(root / 'analysis/interface/dynamic' / asset['file']).convert('RGBA'), tuple(blit['destination']))
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
