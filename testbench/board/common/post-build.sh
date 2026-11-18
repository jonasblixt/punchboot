#!/bin/sh
#
# post-build.sh <target dir>
#
# With PB_BOOT_TIME_MARK=1 (make BOARD=<board> PB_BOOT_TIME_MARK=1) the first
# line of /etc/inittab prints the kernel's uptime as "initmark <seconds>" on
# the console when BusyBox init starts: scripts/boottime measure times the
# boot to it. Without it the line is removed again.

set -eu

INITTAB="$1/etc/inittab"
MARK="::sysinit:/bin/sh -c 'read u _ < /proc/uptime; echo \"initmark \$u\" > /dev/console'"

grep -v "initmark" "$INITTAB" > "$INITTAB.tmp" || true

if [ "${PB_BOOT_TIME_MARK:-0}" = 1 ]; then
    { echo "$MARK"; cat "$INITTAB.tmp"; } > "$INITTAB"
    rm "$INITTAB.tmp"
    echo "post-build: boot time mark in /etc/inittab"
else
    mv "$INITTAB.tmp" "$INITTAB"
fi
