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
- `android/`: the Android adaptive icon and resources (see Android below).
- `png/`: the PNG exports of all of the above.

The logo and banner SVGs have their text converted to paths, so they don't need
the font installed.

## Android

`android/` has the adaptive icon (Android 8+, the APK's minimum) and a `res/` tree
ready to copy into the APK's resources:

- `ic_launcher_background.svg`, `ic_launcher_foreground.svg`: the two layers, on
  the 108 dp canvas. Launchers show the middle 72 dp through their own mask
  (circle, squircle, rounded square); everything that matters stays inside the
  centered 66 dp circle, the part no mask cuts.
- `ic_launcher_monochrome.svg`: the themed icon (Android 13+), one color; the
  system tints it by alpha.
- `res/mipmap-{mdpi..xxxhdpi}/`: the three layers as PNGs (108 to 432 px).
- `res/mipmap-anydpi-v26/ic_launcher.xml`: background + foreground.
  `res/mipmap-anydpi-v33/ic_launcher.xml`: the same plus monochrome, which is
  an API 33 element, so linking against an older android.jar still works.
- `res/drawable-xhdpi/banner.png`: the TV banner (`android:banner`).

With `android:icon="@mipmap/ic_launcher"` in the manifest. No legacy
`ic_launcher.png` is needed: with minSdk 26 the adaptive one is always used.

    inkscape android/ic_launcher_foreground.svg -w 432 -h 432 -o android/res/mipmap-xxxhdpi/ic_launcher_foreground.png

(108 mdpi, 162 hdpi, 216 xhdpi, 324 xxhdpi, 432 xxxhdpi.)

The tile is the background layer; the d-pad and the buttons, with their
shadow, are the foreground, centered.

## Colors

From the BLUE theme (the default) (`src/piccolo_settings.cpp`), plus one warm accent:

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
