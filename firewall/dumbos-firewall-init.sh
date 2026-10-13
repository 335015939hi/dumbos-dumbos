#!/system/bin/sh

IPTABLES="iptables-wrapper-1.0 ip6tables-wrapper-1.0"

for a in $IPTABLES; do
  $a -w -F oem_out
  $a -w -A oem_out -j REJECT
done

for IPT in $IPTABLES; do
  "$IPT" -w -N oem_sbox
  # Rebuild its rules.
  "$IPT" -w -F oem_sbox
  # Root bypasses the interface restriction.
  "$IPT" -w -A oem_sbox \
    -m owner --uid-owner 0 -j RETURN
  # Allow packets routed through sing-box's TUN interface.
  "$IPT" -w -A oem_sbox \
    -o tun0 -j RETURN

  # Block all other non-root outgoing traffic.
  "$IPT" -w -A oem_sbox -j DROP

  # Put the guard before your existing OUTPUT rules.
done

#wait for package manager to become available
sleep 7
exec /system/bin/dumbos-firewall.sh
