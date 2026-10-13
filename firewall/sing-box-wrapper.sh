#!/system/bin/sh

set -eu

# this should match dumbosd-wrapper.sh
oldlogdir="/data/local/tmp/dumb/logs"

dir="/data/local/tmp/dumb/singbox"
allowlist="$dir/whitelist"
sysallowlist="/system/etc/sing-box-whitelist"
singboxconfig="$dir/sing-box.json"
singboxconfigtmp="$dir/sing-box.json.tmp"
logfile="$dir/singbox.log"

mkdir -p "$dir"
mkdir -p "$oldlogdir"

if [[ -f "$logfile" ]]; then
  mv "$logfile" "$oldlogdir/sing-box-$(date '+%Y%m%d-%H%M%S').log"
fi

if [[ ! -f "$allowlist" ]]; then
  touch "$allowlist"
fi

exec 1>"$logfile" 2>&1

cat <<'EOF' >"$singboxconfigtmp"
{
  "$schema": "https://sing-box.sagernet.org/schema.json",
  "log": {
    "level": "info",
    "timestamp": true
  },
  "dns": {
    "servers": [
      {
        "type": "udp",
        "tag": "dns-upstream",
        "server": "8.8.8.8",
        "server_port": 53
      }
    ],
    "final": "dns-upstream"
  },
  "inbounds": [
    {
      "type": "tun",
      "tag": "tun-in",
      "address": "172.19.0.1/30",
      "auto_route": true,
      "platform": {
        "http_proxy": {
          "server": "",
          "server_port": 0
        }
      }
    }
  ],
  "outbounds": [
    {
      "type": "direct",
      "tag": "direct-out"
    }
  ],
  "route": {
    "rules": [
      {
        "action": "sniff"
      },
      {
        "protocol": "dns",
        "action": "hijack-dns"
      },
      {
        "domain": [
EOF

for line in $(cat "$allowlist" "$sysallowlist" | sort -u); do
  echo "\"$line\"," >>"$singboxconfigtmp"
done

cat <<'EOF' >>"$singboxconfigtmp"
        ],
        "domain_suffix": [
        ],
        "ip_cidr": [
          "10.0.2.2"
        ],
        "outbound": "direct-out"
      },
      {
        "action": "reject"
      }
    ],
    "final": "direct-out",
    "auto_detect_interface": true
  }
}
EOF

/system/bin/mv "$singboxconfigtmp" "$singboxconfig"

exec /system/bin/sing-box -c "$singboxconfig" run
