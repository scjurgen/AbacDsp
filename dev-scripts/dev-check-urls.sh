#!/bin/bash
# Check that documentation reference URLs actually resolve.
#
# Usage:
#   dev-check-urls.sh                  # every http(s) URL in src/includes and WEB-REFERENCES.md
#   dev-check-urls.sh URL [URL...]     # only the URLs given on the command line
#
# Prints "CODE  URL" per line, worst first, and exits non-zero if anything is
# not a 2xx. Full result list is kept in build-tests/scratch/url-check.txt.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_env.sh"

TIMEOUT=25
JOBS=8
# Some hosts (Cloudflare in front of academic pages, mainly) refuse curl's default agent.
UA='Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0 Safari/537.36'

check_one()
{
    local url="$1" code
    # HEAD is cheapest, but a fair number of servers answer 403 or 405 to it,
    # so fall back to a one-byte ranged GET before believing a failure.
    code="$(curl -s -o /dev/null -w '%{http_code}' -A "$UA" -I -L --max-time "$TIMEOUT" "$url" || true)"
    case "$code" in
        2*) ;;
        *) code="$(curl -s -o /dev/null -w '%{http_code}' -A "$UA" -r 0-0 -L --max-time "$TIMEOUT" "$url" || true)" ;;
    esac
    printf '%s  %s\n' "${code:-000}" "$url"
}
export -f check_one
export UA TIMEOUT

collect_urls()
{
    grep -rhoE 'https?://[^][ )"<>`'"'"']+' "$ROOT_DIR/src/includes" --include='*.h' || true
    if [ -f "$ROOT_DIR/WEB-REFERENCES.md" ]; then
        grep -hoE 'https?://[^][ )"<>`'"'"']+' "$ROOT_DIR/WEB-REFERENCES.md" || true
    fi
}

LIST="$SCRATCH_DIR/url-list.txt"
RESULTS="$SCRATCH_DIR/url-check.txt"

if [ "$#" -gt 0 ]; then
    printf '%s\n' "$@" > "$LIST"
else
    collect_urls | sed -e 's/[.,;:]*$//' | sort -u > "$LIST"
fi

COUNT="$(wc -l < "$LIST" | tr -d ' ')"
if [ "$COUNT" -eq 0 ]; then
    echo "no URLs found"
    exit 0
fi
echo "checking $COUNT URL(s)..."

xargs -P "$JOBS" -I {} bash -c 'check_one "$@"' _ {} < "$LIST" | sort > "$RESULTS"

# 2xx last, so the interesting lines end up nearest the prompt.
grep -v '^2' "$RESULTS" || true
grep '^2' "$RESULTS" || true

BAD="$(grep -cv '^2' "$RESULTS" || true)"
BAD="${BAD:-0}"
echo "---"
echo "$((COUNT - BAD))/$COUNT ok, full list: $RESULTS"
if [ "$BAD" -ne 0 ]; then
    echo "$BAD URL(s) did not return 2xx"
    exit 1
fi
