#!/bin/sh
# Kill leftover tdx/tdxview and their sockets. Always run before a new pair.
set -e
pkill -x tdx 2>/dev/null || true
pkill -x tdxview 2>/dev/null || true
# ASan leak abort on exit can outlive pkill; wait until gone.
i=0
while [ "$i" -lt 20 ]; do
    if ! pgrep -x tdx >/dev/null && ! pgrep -x tdxview >/dev/null; then
        break
    fi
    i=$((i + 1))
    sleep 0.2
done
pkill -9 -x tdx 2>/dev/null || true
pkill -9 -x tdxview 2>/dev/null || true
rm -f /tmp/tdx.sock /tmp/tdxview.sock
if pgrep -x tdx >/dev/null || pgrep -x tdxview >/dev/null; then
    echo "tdx-kill: still running:" >&2
    ps -eo pid,cmd | grep -E '[.]/tdx(view)?( |$)' | grep -v grep >&2 || true
    exit 1
fi
echo "tdx-kill: no tdx/tdxview"
exit 0
