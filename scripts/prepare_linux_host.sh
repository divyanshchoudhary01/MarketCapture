#!/usr/bin/env bash
set -euo pipefail
cpu="${CPU:-2}"
hugepages="${HUGEPAGES:-1024}"
test "$(id -u)" -eq 0 || { echo "run as root" >&2; exit 2; }
test -d "/sys/devices/system/cpu/cpu$cpu" || { echo "invalid CPU $cpu" >&2; exit 2; }
if test -w "/sys/devices/system/cpu/cpu$cpu/cpufreq/scaling_governor"; then
  echo performance > "/sys/devices/system/cpu/cpu$cpu/cpufreq/scaling_governor"
fi
sysctl -w "vm.nr_hugepages=$hugepages"
mkdir -p /mnt/huge
mountpoint -q /mnt/huge || mount -t hugetlbfs nodev /mnt/huge
echo "CPU $cpu set to performance where supported; $hugepages huge pages requested."
echo "CPU isolation (isolcpus/nohz_full/rcu_nocbs) requires bootloader configuration."
