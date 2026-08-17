#!/system/bin/sh

#firewall controller.
#reads config file at /data/local/tmp/dumb/firewall.conf and /system/etc/dumb-firewall.conf

CONFFILE=/data/local/tmp/dumb/dumbos-firewall.conf
DEFAULT_CONFFILE=/system/etc/dumbos-firewall.conf

IPTABLES="/system/bin/iptables-wrapper-1.0 /system/bin/ip6tables-wrapper-1.0"
CHAIN="oem_out"

#hardcoded list of allowed uids, e.g. root (uid 0)
ALLOWED_UIDS="0"

die() {
  echo "$@"
  if [ -f "$TMPFILE" ]; then
    rm -f "$TMPFILE"
  fi
  exit 1
}

firewall_allow() {
  for ip in $IPTABLES; do
    "$ip" -w -I "$CHAIN" -m owner --uid-owner "$1" -j RETURN || return $?
  done
}

for ip in $IPTABLES; do
  "$ip" -w -F "$CHAIN" || die "$ip -w -F $CHAIN failed:$?"
  "$ip" -w -A "$CHAIN" -j REJECT || die "$ip -w -A $CHAIN -j REJECT failed:$?"
done

if ! [ -f "$DEFAULT_CONFFILE" ]; then
  echo "Error:default config '$DEFAULT_CONFFILE' does not exist. aborting."
  exit 1
fi
if [ -f "$CONFFILE" ]; then
  PACKAGES="$(cat "$DEFAULT_CONFFILE" "$CONFFILE")" || die "failed to read config files $DEFAULT_CONFFILE $CONFFILE"
else
  echo "'$CONFFILE' does not exist"
  PACKAGES="$(cat "$DEFAULT_CONFFILE")" || die "failed to read default config $DEFAULT_CONFFILE"
fi

TMPFILE="$(mktemp)" || die "mktemp failed:$?"

pm list packages --user 0 -U >"$TMPFILE" || die "pm list packages failed:$?"

echo "allowed packages are:\n$PACKAGES"
for package in $PACKAGES; do
  line="$(grep -F -m 1 "package:$package " "$TMPFILE")"
  if [ "$line" "=" "" ]; then
    echo "package '$package' does not exist"
  else
    #appuid="$(echo "$appuid" | sed 's/^package:.* uid://')"
    appuid="${line##* uid:}"
    echo "allowing package '$package' with uid '$appuid'"
    firewall_allow "$appuid" || echo "allow failed with $?"
  fi
done

for uid in $ALLOWED_UIDS; do
  echo "allowing uid $uid"
  firewall_allow "$uid" || echo "allow failed:$?"
done

rm "$TMPFILE"
