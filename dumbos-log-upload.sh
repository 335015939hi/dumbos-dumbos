#!/system/bin/sh

set -eu

# these should match dumbosd-wrapper.sh
tmpdir=/data/local/tmp/dumb
oldlogdir="$tmpdir"/logs

sleep 60

for path in "$oldlogdir"/*; do
    [ -e "$path" ] || [ -L "$path" ] || continue
    (/system/bin/dumbos upload_data -- "$path" && /system/bin/rm -- "$path") || true
done
