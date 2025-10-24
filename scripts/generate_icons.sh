#!/bin/bash
# Generate icon variants for the NMBS Pebble app
# Requires ImageMagick (magick command)
#
# Usage: ./generate_icons.sh <input_icon.png>
# Example: ./generate_icons.sh switch.png

set -e  # Exit on error

# Check if input file is provided
if [ $# -eq 0 ]; then
    echo "Error: No input file specified"
    echo "Usage: $0 <input_icon.png>"
    echo "Example: $0 switch.png"
    exit 1
fi

INPUT_FILE="$1"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RESOURCES_DIR="$SCRIPT_DIR/../resources"

# Check if ImageMagick is installed
if ! command -v magick &> /dev/null; then
    echo "Error: ImageMagick is not installed. Install it with: brew install imagemagick"
    exit 1
fi

# Check if input file exists
if [ ! -f "$RESOURCES_DIR/$INPUT_FILE" ]; then
    echo "Error: Input file '$INPUT_FILE' not found in resources directory"
    exit 1
fi

cd "$RESOURCES_DIR"

# Get filename without extension
BASENAME="${INPUT_FILE%.*}"
EXTENSION="${INPUT_FILE##*.}"

echo "Generating icon variants for '$INPUT_FILE'..."

echo "  - Generating ${BASENAME}_white.$EXTENSION (inverted colors with transparency)..."
magick "$INPUT_FILE" -channel RGB -negate +channel "${BASENAME}_white.$EXTENSION"

echo "  - Generating ${BASENAME}_1bit.$EXTENSION (monochrome with white background)..."
magick "$INPUT_FILE" -background white -alpha remove -flatten -monochrome -depth 1 "${BASENAME}_1bit.$EXTENSION"

echo "✓ Icon variants generated successfully!"
echo ""
echo "Generated files:"
echo "  - ${BASENAME}_white.$EXTENSION (white icon with transparency)"
echo "  - ${BASENAME}_1bit.$EXTENSION (1-bit monochrome)"
