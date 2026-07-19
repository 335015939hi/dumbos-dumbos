#!/system/bin/sh

IPTABLES="iptables-wrapper-1.0 ip6tables-wrapper-1.0"

for a in $IPTABLES; do
  $a -w -F oem_out
  $a -w -A oem_out -j REJECT
done

#wait for package manager to become available
sleep 7
exec /system/bin/dumbos-firewall.sh
