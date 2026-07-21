#!/system/bin/sh

# Controller for the dumbos firewall configuration file.

CONFFILE=/data/local/tmp/dumb/dumbos-firewall.conf

POLICY_ALLOW=allow
POLICY_DENY=deny

RETURN_CODE=0
TMPFILE=

display_help() {
  echo "Usage: $0 <policy> <appId> [appId2 ...]"
  echo "<policy> is either $POLICY_ALLOW or $POLICY_DENY"
  echo "<appId> is the package ID of each targeted app"
  echo "$POLICY_ALLOW adds the target app to the firewall whitelist"
  echo "$POLICY_DENY removes the target app from the firewall whitelist"
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
  [ "$POLICY" != "$POLICY_DENY" ]; then
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
  package=$1

  if [ -e "$CONFFILE" ]; then
    grep -Fqx -- "$package" "$CONFFILE"
    status=$?

    case $status in
    0)
      echo "Warning: package '$package' already exists" >&2
      return 1
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

remove_package() {
  package=$1

  if [ ! -f "$CONFFILE" ]; then
    echo "Warning: package '$package' does not exist" >&2
    return 1
  fi

  grep -Fqx -- "$package" "$CONFFILE"
  status=$?

  case $status in
  0)
    ;;
  1)
    echo "Warning: package '$package' does not exist" >&2
    return 1
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

  if ! cat -- "$TMPFILE" >"$CONFFILE"; then
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
  else
    remove_package "$app" || RETURN_CODE=1
  fi
done

dumbos-firewall.sh

cleanup
exit "$RETURN_CODE"
