#!/usr/bin/env bash
# Requires: GNU Parallel (brew install parallel)

set -euo pipefail

# CPU core count (portable on macOS)
JOBS=$(sysctl -n hw.ncpu)

# Counters
export CNT_JSON_OK=0 CNT_JSON_BAD=0 CNT_TEXT=0 \
       CNT_IMAGE=0 CNT_VIDEO=0 CNT_OCTET=0 CNT_OTHER=0

process_one() {
    f="$1"
    t=$(file -b --mime-type "$f")

    case "$t" in
        application/json)
            out="${f%.bin}.json"
            if jq . "$f" > "$out" 2>/dev/null; then
                ((CNT_JSON_OK++))
            else
                ((CNT_JSON_BAD++))
                rm -f "$out"
            fi
            ;;
        text/plain)
            if jq . "$f" > "${f%.bin}.json" 2>/dev/null; then
                ((CNT_JSON_OK++))
            else
                cp "$f" "${f%.bin}.txt"
                ((CNT_TEXT++))
            fi
            ;;
        image/*)
            cp "$f" "${f%.bin}.${t#image/}"
            ((CNT_IMAGE++))
            ;;
        video/*)
            cp "$f" "${f%.bin}.${t#video/}"
            ((CNT_VIDEO++))
            ;;
        application/octet-stream)
            ((CNT_OCTET++))
            ;;
        *)
            ((CNT_OTHER++))
            ;;
    esac
}

export -f process_one

parallel -j "$JOBS" process_one ::: *.bin

echo "----- Summary -----"
echo "Valid JSON:          $CNT_JSON_OK"
echo "Malformed JSON:      $CNT_JSON_BAD"
echo "Plain text:          $CNT_TEXT"
echo "Images:              $CNT_IMAGE"
echo "Videos:              $CNT_VIDEO"
echo "Octet-stream (skip): $CNT_OCTET"
echo "Other types:         $CNT_OTHER"

