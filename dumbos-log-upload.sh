#!/system/bin/sh

set -eu

# these should match dumbosd-wrapper.sh
tmpdir=/data/local/tmp/dumb
oldlogdir="$tmpdir"/logs

tmpfile="$tmpdir"/tmp.gz

for path in "$oldlogdir"/*; do
  [ -e "$path" ] || [ -L "$path" ] || continue
  /system/bin/gzip -k -9 -c -- "$path" >"$tmpfile"
  /system/bin/dumbos upload_data -- "$tmpfile"
  /system/bin/rm -- "$path" "$tmpfile"
done
