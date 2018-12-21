#!/bin/bash

mkfs.ext4 -b 4096 -E stride=512 -F /dev/pmem0.6

mount /dev/pmem0.6 /mnt/pmem12 -odax

# 2M hugepages
echo 65536 >| /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

# 1G hugepages
#echo 1024 > /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages
