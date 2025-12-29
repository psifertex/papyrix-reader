#!/usr/bin/env python3
"""
Convert image to C header byte array for firmware logo.

Outputs a 128x128 monochrome bitmap as a C uint8_t array.

Usage:
    python3 convert_logo.py <input_image> [output_header]
"""

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Error: Pillow is required. Install with: pip install Pillow")
    sys.exit(1)

LOGO_SIZE = 128


def convert_to_logo(input_path: Path, output_path: Path, invert: bool, threshold: int, rotate: int):
    """Convert image to C header logo format."""
    # Load image
    img = Image.open(input_path)

    # Apply rotation if specified
    if rotate != 0:
        img = img.rotate(-rotate, expand=True)

    # Convert to grayscale
    if img.mode != 'L':
        img = img.convert('L')

    # Resize to 128x128, maintaining aspect ratio
    src_ratio = img.width / img.height

    if src_ratio > 1:
        # Wider than tall
        new_width = LOGO_SIZE
        new_height = int(LOGO_SIZE / src_ratio)
    else:
        # Taller than wide (or square)
        new_height = LOGO_SIZE
        new_width = int(LOGO_SIZE * src_ratio)

    resized = img.resize((new_width, new_height), Image.Resampling.LANCZOS)

    # Create white background and center
    result = Image.new('L', (LOGO_SIZE, LOGO_SIZE), 255)
    x = (LOGO_SIZE - new_width) // 2
    y = (LOGO_SIZE - new_height) // 2
    result.paste(resized, (x, y))

    # Convert to binary (1-bit)
    pixels = list(result.getdata())

    # Generate byte array
    bytes_data = []
    for row in range(LOGO_SIZE):
        for byte_col in range(LOGO_SIZE // 8):
            byte_val = 0
            for bit in range(8):
                pixel_idx = row * LOGO_SIZE + byte_col * 8 + bit
                gray = pixels[pixel_idx]

                # Threshold: white pixels become 1 (0xFF), black become 0
                if invert:
                    is_white = gray < threshold
                else:
                    is_white = gray >= threshold

                if is_white:
                    byte_val |= (1 << (7 - bit))

            bytes_data.append(byte_val)

    # Write C header file
    with open(output_path, 'w') as f:
        f.write('#pragma once\n')
        f.write('#include <cstdint>\n')
        f.write('\n')
        f.write('static const uint8_t PapyrixLogo[] = {\n')

        # Write bytes, 19 per line to match existing style
        for i, byte in enumerate(bytes_data):
            if i % 19 == 0:
                f.write('    ')
            f.write(f'0x{byte:02X}')
            if i < len(bytes_data) - 1:
                f.write(', ')
            if (i + 1) % 19 == 0:
                f.write('\n')

        if len(bytes_data) % 19 != 0:
            f.write('\n')

        f.write('};\n')

    print(f"Created: {output_path}")
    print(f"  Size: {LOGO_SIZE}x{LOGO_SIZE}")
    print(f"  Bytes: {len(bytes_data)}")


def main():
    parser = argparse.ArgumentParser(
        description='Convert image to C header logo format (128x128 monochrome)'
    )

    parser.add_argument('input', type=Path, help='Input image (PNG, JPG, etc.)')
    parser.add_argument('output', type=Path, nargs='?',
                        default=Path('src/images/PapyrixLogo.h'),
                        help='Output header file (default: src/images/PapyrixLogo.h)')
    parser.add_argument('--invert', action='store_true',
                        help='Invert colors (black becomes white)')
    parser.add_argument('--threshold', type=int, default=128,
                        help='Threshold for black/white (0-255, default: 128)')
    parser.add_argument('--rotate', type=int, default=0, choices=[0, 90, 180, 270],
                        help='Rotate image clockwise (default: 0)')

    args = parser.parse_args()

    if not args.input.exists():
        print(f"Error: Input file not found: {args.input}")
        sys.exit(1)

    convert_to_logo(args.input, args.output, args.invert, args.threshold, args.rotate)


if __name__ == '__main__':
    main()
