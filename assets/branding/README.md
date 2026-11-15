# piccolo branding

The name is always lowercase: piccolo.

## Files

- `piccolo-icon.svg`: app icon (48 px and up), with its inner shadow.
- `piccolo-icon-small.svg`: 24 and 32 px, simpler shapes, no shadow.
- `piccolo-icon-16.svg`: 16 px, drawn on the pixel grid.
- `piccolo-logo-dark.svg`, `piccolo-logo-light.svg`: icon + wordmark, transparent
  background, for dark and light pages.
- `piccolo-banner-tv-dark.svg`, `piccolo-banner-tv-light.svg`: Android TV banner,
  drawn at 640x360, exported at 320x180 (xhdpi). Dark is the one for the launcher.
- `png/`: the PNG exports of all of the above.

The logo and banner SVGs have their text converted to paths, so they don't need
the font installed.

## Colors

From the PICCOLO theme (`src/piccolo_settings.cpp`), plus one warm accent:

| Use                 | Dark page | Light page |
|---------------------|-----------|------------|
| Text                | `#E0E6F0` | `#1A2840`  |
| Hard shadow (6/150) | `#101828` | `#B4C2DC`  |
| Background          | `#2A4068` | `#E8EDF5`  |
| Icon tile           | `#1A2840` | `#1A2840`  |
| Accent (first «o»)  | `#FFC93C` | `#FFC93C`  |

## Type

Fredoka SemiBold (600), letter-spacing -0.02em (OFL, Google Fonts). The first
«o» is a yellow disc the size of the x-height, sitting on the baseline.

## Regenerating

PNGs, with Inkscape:

    inkscape piccolo-icon.svg -w 256 -h 256 -o png/piccolo-icon-256.png

48-512 from `piccolo-icon.svg`, 24-32 from `piccolo-icon-small.svg`, 16 from
`piccolo-icon-16.svg`. Then embed the window icon:

    python3 tools/make_app_icon.py
