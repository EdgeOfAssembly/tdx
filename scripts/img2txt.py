#!/usr/bin/env python3
"""img2txt -- convert an image to plain ASCII art on stdout.

A thin, dependency-light wrapper over the proven pipeline in /mnt/ascii's
ascii_utils (load_and_preprocess_image -> optional Floyd-Steinberg dither
-> map_to_ascii). It stops at the *text* stage: no Pillow rendering, no
video assembly, no ANSI colour by default -- just characters a text-only
model can "see".

Install system-wide later by copying this file plus ascii_utils.py, or by
adding /mnt/ascii to PYTHONPATH and this script's dir to PATH.

Usage
-----
    img2txt IMAGE [-w WIDTH] [--no-dither] [--color 24|8|none] [--invert]
                  [--charset CHARS] [--sigma S] [--sharpen K] [--bias B]

Examples
--------
    img2txt /tmp/shit.png -w 160
    img2txt shot.png -w 120 --no-dither
    img2txt ui.bmp -w 200 --color 24      # truecolour ANSI (for humans)
"""

import argparse
import sys
from pathlib import Path

# Pull in the shared conversion core.
_ASCII_HOME = Path("/mnt/ascii")
if str(_ASCII_HOME) not in sys.path:
    sys.path.insert(0, str(_ASCII_HOME))

import numpy as np  # noqa: E402

from ascii_utils import (  # noqa: E402
    load_and_preprocess_image,
    floyd_steinberg_dither,
    map_to_ascii,
    DEFAULT_PALETTE,
)


def image_to_ascii(
    image_path: str,
    width: int = 120,
    sigma: float = 0.5,
    sharpen: float = 0.3,
    bias: float = 0.0,
    use_dither: bool = True,
    palette: str = DEFAULT_PALETTE,
    colors: str = "none",
    invert: bool = False,
    grayscale: bool = True,
) -> str:
    """Return *image_path* rendered as an ASCII string (no rendering to file)."""
    _, rgb_color, _ = load_and_preprocess_image(
        image_path, width, sigma=sigma, sharpen=sharpen, white_balance=None
    )

    # load_and_preprocess_image returns the enhanced (sharpened) gray in
    # slot 0 only in the GPU path tuple ordering differs; recompute the
    # processed luminance deterministically from rgb to be safe.
    rgb = rgb_color
    gray = np.dot(rgb[..., :3], [0.299, 0.587, 0.114])
    gray = gray / np.max(gray) if np.max(gray) > 0 else gray

    processed = np.clip(gray + bias, 0, 1)
    if invert:
        processed = 1.0 - processed

    if use_dither:
        mapped = floyd_steinberg_dither(processed.copy(), len(palette))
    else:
        mapped = processed

    color_rgb = None if grayscale else rgb_color
    return map_to_ascii(processed, mapped, palette, colors, use_dither, rgb_color=color_rgb)


def main(argv=None) -> int:
    p = argparse.ArgumentParser(
        description="Convert an image to ASCII art on stdout (text-only, for models).",
    )
    p.add_argument("image", help="Input image path (png/jpg/bmp/...)")
    p.add_argument("-w", "--width", type=int, default=120,
                   help="Width in characters (default 120)")
    p.add_argument("--sigma", type=float, default=0.5, help="Gaussian blur sigma")
    p.add_argument("--sharpen", type=float, default=0.3, help="Sharpen strength")
    p.add_argument("--bias", type=float, default=0.0, help="Brightness bias [-1,1]")
    p.add_argument("--no-dither", action="store_true", help="Disable Floyd-Steinberg dithering")
    p.add_argument("--invert", action="store_true", help="Invert light/dark")
    p.add_argument("--color", choices=["none", "8", "24"], default="none",
                   help="Colour mode for the OUTPUT text (default none = plain)")
    p.add_argument("--rgb", action="store_true",
                   help="Use true per-pixel colour with --color 24 (else grayscale ramp)")
    p.add_argument("--charset", type=str, default=DEFAULT_PALETTE,
                   help="Character ramp, dark-to-light")
    args = p.parse_args(argv)

    if not Path(args.image).is_file():
        p.error(f"no such image: {args.image}")

    art = image_to_ascii(
        args.image,
        width=args.width,
        sigma=args.sigma,
        sharpen=args.sharpen,
        bias=args.bias,
        use_dither=not args.no_dither,
        palette=args.charset,
        colors=args.color,
        grayscale=not args.rgb,
    )
    sys.stdout.write(art)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
