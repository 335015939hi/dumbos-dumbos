#!/system/bin/sh

# Controller for the dumbos firewall configuration file.

CONFFILE=/data/local/tmp/dumb/dumbos-firewall.conf

POLICY_ALLOW=allow
POLICY_DENY=deny
POLICY_ALLOW_TEMP=allow-temp

RETURN_CODE=0
TMPFILE=

display_help() {
  echo "Usage: $0 <policy> <appId> [appId2 ...]"
  echo "<policy> is either $POLICY_ALLOW or $POLICY_DENY or $POLICY_ALLOW_TEMP"
  echo "<appId> is the package ID of each targeted app"
  echo "$POLICY_ALLOW adds the target app to the firewall whitelist"
  echo "$POLICY_DENY removes the target app from the firewall whitelist"
  echo "$POLICY_ALLOW_TEMP allows the target app until the next time firewall is reloaded (either through another $0 command or reboot)"
  echo "WARNING: Internet access is controlled by UID. Apps sharing a UID (looking at you, system apps, but some other apps as well) will therefore share the same firewall access."
}

cleanup() {
  if [ -n "$TMPFILE" ]; then
    rm -f -- "$TMPFILE"
  fi
}

die() {
  echo "$@" >&2
  cleanup
  exit 1
}

if [ "$#" -lt 2 ]; then
  display_help
  exit 1
fi

POLICY=$1

if [ "$POLICY" != "$POLICY_ALLOW" ] &&
  [ "$POLICY" != "$POLICY_DENY" ] && [ "$POLICY" != "$POLICY_ALLOW_TEMP" ]; then
  echo "Invalid policy: '$POLICY'" >&2
  display_help
  exit 1
fi

shift

check_package_name() {
  case $1 in
  '' | *[!a-zA-Z0-9_.]*)
    return 1
    ;;
  *)
    return 0
    ;;
  esac
}

CONFDIR=${CONFFILE%/*}

mkdir -p -- "$CONFDIR" ||
  die "Failed to create configuration directory: $CONFDIR"

TMPFILE=$(mktemp "$CONFDIR/.dumbos-firewall.conf.XXXXXX") ||
  die "Failed to create temporary file"

add_package() {
  package="$1"
  if [ -e "$CONFFILE" ]; then
    grep -Fqx -- "$package" "$CONFFILE"
    status=$?

    case $status in
    0)
      echo "Warning: package '$package' already exists" >&2
      return 0
      ;;
    1)
      ;;
    *)
      echo "Error: failed to search '$CONFFILE'" >&2
      return 1
      ;;
    esac
  fi

  if ! printf '%s\n' "$package" >>"$CONFFILE"; then
    echo "Error: failed to add package '$package'" >&2
    return 1
  fi

  return 0
}

add_package_temp() {
  package="$1"
  pdata="$(pm list packages --user 0 -U | grep -F -m 1 "package:$package ")" || return $?
  uid="${pdata##* uid:}"
  IPTABLES="/system/bin/iptables-wrapper-1.0 /system/bin/ip6tables-wrapper-1.0"
  for ipt in $IPTABLES; do
    "$ipt" -w -I oem_out -m owner --uid-owner "$uid" -j RETURN || return $?
  done
}

remove_package() {
  package="$1"

  if [ ! -f "$CONFFILE" ]; then
    echo "Warning: package '$package' does not exist" >&2
    return 0
  fi

  grep -Fqx -- "$package" "$CONFFILE"
  status=$?

  case $status in
  0)
    ;;
  1)
    echo "Warning: package '$package' does not exist" >&2
    return 0
    ;;
  *)
    echo "Error: failed to search '$CONFFILE'" >&2
    return 1
    ;;
  esac

  grep -Fvx -- "$package" "$CONFFILE" >"$TMPFILE"
  status=$?

  # grep returns 1 when no lines remain. That is valid here.
  if [ "$status" -gt 1 ]; then
    echo "Error: failed to filter '$CONFFILE'" >&2
    return 1
  fi

  if ! mv "$TMPFILE" "$CONFFILE"; then
    echo "Error: failed to update '$CONFFILE'" >&2
    return 1
  fi

  return 0
}

for app in "$@"; do
  echo "Processing app '$app'"

  if ! check_package_name "$app"; then
    echo "Warning: '$app' is an invalid package ID" >&2
    RETURN_CODE=1
    continue
  fi

  if [ "$POLICY" = "$POLICY_ALLOW" ]; then
    add_package "$app" || RETURN_CODE=1
  elif [ "$POLICY" = "$POLICY_DENY" ]; then
    remove_package "$app" || RETURN_CODE=1
  else
    add_package_temp "$app" || {
      RETURN_CODE=1
      echo "Warning: failed processing '$app'"
    }
  fi
done

#don't reload firewall if alllowing temporary
if [ "$POLICY" != "$POLICY_ALLOW_TEMP" ]; then
  dumbos-firewall.sh
fi

cleanup
exit "$RETURN_CODE"
