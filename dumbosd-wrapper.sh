#!/system/bin/sh

set -eu

tmpdir=/data/local/tmp/dumb

logdir="$tmpdir"/log
oldlogdir="$tmpdir"/logs

logfile="$logdir/$(/system/bin/date '+%Y%m%d-%H%M%S').log"

/system/bin/mkdir -p -- "$logdir"
/system/bin/mkdir -p -- "$tmpdir"
/system/bin/mkdir -p -- "$oldlogdir"

for path in "$logdir"/*; do
  [ -e "$path" ] || [ -L "$path" ] || continue
  /system/bin/mv -- "$path" "$oldlogdir"/
done

exec >>"$logfile" 2>&1

exec /system/bin/dumbosd --tmpdir="$tmpdir"
