#!/usr/bin/env bash
# Regenerates the LVGL font C files in this directory from IBM Plex.
#
# Fonts are compiled into the firmware at fixed pixel sizes (ADR-0008): LVGL
# cannot scale a bitmap font, so every size and weight the UI uses is its own
# C array. Add a line below when the UI needs a new one, rerun, and commit the
# generated .c files -- the build never runs this script.
#
# SOUP: IBM Plex Sans 1.1.0 and IBM Plex Mono 2.5.0 (npm @ibm/plex-sans,
# @ibm/plex-mono), OFL 1.1, converted with lv_font_conv 1.5.3.
#
# Usage: components/ui/fonts/gen_fonts.sh   (needs node/npx and network)

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

PLEX_SANS=1.1.0
PLEX_MONO=2.5.0
LV_FONT_CONV=1.5.3

(cd "$WORK" && npm pack "@ibm/plex-sans@$PLEX_SANS" "@ibm/plex-mono@$PLEX_MONO" --silent >/dev/null)
mkdir -p "$WORK/sans" "$WORK/mono"
tar xzf "$WORK/ibm-plex-sans-$PLEX_SANS.tgz" -C "$WORK/sans"
tar xzf "$WORK/ibm-plex-mono-$PLEX_MONO.tgz" -C "$WORK/mono"
SANS="$WORK/sans/package/fonts/complete/woff/IBMPlexSans"
MONO="$WORK/mono/package/fonts/complete/woff/IBMPlexMono"

# Printable ASCII plus the symbols ADR-0008 requires: Ω θ Δ × — · µ ≤ ≥,
# – (en dash, used in ranges such as "0.5 – 50 kΩ") and ² (cd/m²).
TEXT_RANGE="0x20-0x7E,0xB2,0xB5,0xB7,0xD7,0x394,0x3A9,0x3B8,0x2013,0x2014,0x2264,0x2265"

# IBM Plex Mono has no Greek: Δ Ω θ are missing in every weight. The Mono fonts
# take everything else from Mono and those three glyphs from Plex Sans of the
# same weight, merged into one LVGL font.
MONO_RANGE="0x20-0x7E,0xB2,0xB5,0xB7,0xD7,0x2013,0x2014,0x2264,0x2265"
GREEK_RANGE="0x394,0x3A9,0x3B8"

# The very large value fonts only ever show numbers, so they carry digits and
# the few characters a number display needs. Keeps them small in flash.
DIGITS_SYMBOLS="0123456789.-—– "

conv() {  # name size  then one or more: file range-or-symbols-flag value
    local name="$1" size="$2"
    shift 2
    local sources=()
    while (($#)); do
        sources+=(--font "$1.woff" "$2" "$3")
        shift 3
    done
    npx --yes "lv_font_conv@$LV_FONT_CONV" --no-compress --no-prefilter \
        --bpp 4 --size "$size" --format lvgl --lv-include lvgl.h \
        "${sources[@]}" \
        -o "$HERE/$name.c" --lv-font-name "$name"
    # Strip the absolute temp paths lv_font_conv writes into the header, so
    # regenerating on another machine gives an identical file.
    sed -i.bak -e "s#$WORK/[a-z]*/package/fonts/complete/woff/##g" -e "s#$HERE/##g" "$HERE/$name.c"
    rm -f "$HERE/$name.c.bak"
    echo "  $name"
}

echo "Generating fonts into $HERE"
# Tier 3 (ADR-0008): labels, button text.
conv plex_sans_32           32  "$SANS-Regular"  --range "$TEXT_RANGE"
conv plex_sans_32_semibold  32  "$SANS-SemiBold" --range "$TEXT_RANGE"
conv plex_mono_32           32  "$MONO-Regular"  --range "$MONO_RANGE" \
                                "$SANS-Regular"  --range "$GREEK_RANGE"
# Tier 2: secondary values, device state.
conv plex_sans_46_medium    46  "$SANS-Medium"   --range "$TEXT_RANGE"
conv plex_sans_46_semibold  46  "$SANS-SemiBold" --range "$TEXT_RANGE"
conv plex_mono_46_medium    46  "$MONO-Medium"   --range "$MONO_RANGE" \
                                "$SANS-Medium"   --range "$GREEK_RANGE"
# Tier 1: Peripheral-Equivalent TOF Ratio, TOF Count, fault title.
conv plex_sans_77_bold      77  "$SANS-Bold"     --range "$TEXT_RANGE"
# Number displays, above the 77 px minimum.
conv plex_mono_120_semibold 120 "$MONO-SemiBold" --symbols "$DIGITS_SYMBOLS"
conv plex_mono_140_semibold 140 "$MONO-SemiBold" --symbols "$DIGITS_SYMBOLS"

cp "$WORK/sans/package/LICENSE.txt" "$HERE/LICENSE-IBM-Plex.txt"
