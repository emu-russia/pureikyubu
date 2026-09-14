#!/usr/bin/env python3
"""Make the Game Boy Advance icon out of the public-domain console photograph.

Source: "Nintendo-Game-Boy-Advance-Purple-FL.png" by Evan-Amos, released into the public domain
(see https://commons.wikimedia.org/wiki/File:Nintendo-Game-Boy-Advance-Purple-FL.png). It is the
console every Game Boy Advance player knows - the purple one - and it is free of licensing
questions, which is why the emulator uses it rather than a stock illustration.

The script crops the console out of the white studio background, turns that background into
transparency (a luminance key: the white paper goes, the light grey buttons stay) and writes two
sizes:

    src/res/gba_icon.png      256x256, for the UI and the Windows resources
    docs/imgstore/gba_icon.png 96x96, for the documentation pages

Usage (needs Pillow, which WSL's python3 has):

    python3 src/res/make_gba_icon.py <the downloaded photograph.png>

The photograph itself is *not* stored in the repository (it is 2.4 MByte and can be fetched from
Wikimedia Commons with the URL above); only the derived icons are.
"""

import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))

# The console in the 1920px-wide photograph: everything else is the white background.
CROP = (110, 90, 1810, 1230)


def key_out_white(image):
    """Return the image with the white background removed (a soft luminance key)."""
    image = image.convert("RGBA")
    pixels = image.load()
    width, height = image.size

    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            luminance = max(r, g, b)

            if luminance < 235:
                continue                    # the console (and its light grey parts) stays opaque

            # Fade the near-white pixels out, so the silhouette keeps a soft edge instead of a
            # jagged one.
            fade = (luminance - 235) / 20.0
            pixels[x, y] = (r, g, b, int(a * (1.0 - min(1.0, fade))))

    return image


def trimmed(image):
    """Drop the fully transparent border the key left behind."""
    box = image.getbbox()
    return image.crop(box) if box else image


def square(image, size):
    """Fit the picture into a square canvas (it is wider than tall) and resize it."""
    image = image.copy()
    image.thumbnail((size, size), Image.LANCZOS)

    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.paste(image, ((size - image.width) // 2, (size - image.height) // 2), image)
    return canvas


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2

    source = Image.open(sys.argv[1])
    image = trimmed(key_out_white(source.crop(CROP)))

    for size, path in ((256, os.path.join(HERE, "gba_icon.png")),
                       (96, os.path.join(REPO, "docs", "imgstore", "gba_icon.png"))):
        square(image, size).save(path)
        print("wrote %s (%dx%d)" % (path, size, size))

    return 0


if __name__ == "__main__":
    sys.exit(main())
