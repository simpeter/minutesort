#!/bin/bash

set -e

QSORT=/home/simon/qsort/qsort
PARTITION=/home/simon/qsort/partition
RECORDS=$((256 * 1024 * 1024))
RECORDSIZE=100
NODES=(localhost)
NUMA_PATHS=(/mnt/pmem12)
NUMA_CORES=32
TIME=
TIMESTAMP=

generate=yes
tencentsort=yes
validate=yes

let n=${#NODES[*]}
let numa_nodes=${#NUMA_PATHS[*]}
let mappers=NUMA_CORES*numa_nodes*n

[ $mappers -gt 1 ] || (echo "mappers ($mappers) must be > 1 !"; exit 1)
[ $((RECORDS % mappers)) = 0 ] || (echo "RECORDS ($RECORDS) % mappers ($mappers) != 0 ($((RECORDS % mappers))) !"; exit 1)

let maprecords=RECORDS/mappers

while getopts gtvhTS opt; do
    case $opt in
	g)
	    generate=no
	    ;;
	t)
	    tencentsort=no
	    ;;
	v)
	    validate=no
	    ;;
	T)
	    TIME=time
	    ;;
	S)
	    TIMESTAMP=date
	    ;;
	h)
	    echo -e "Usage: $0 [-gtvTh]\n\n" \
		 " -g\tDon't generate unsorted input files\n" \
		 " -t\tDon't run TencentSort\n" \
		 " -v\tDon't validate sorted output\n" \
		 " -T\tDisplay timing breakdown\n" \
		 " -S\tDisplay timestamps for debug\n" \
		 " -h\tDisplay help\n"
	    exit
	    ;;
    esac
done

echo "Configuration: generate = $generate, tencentsort = $tencentsort, validate = $validate"

echo "Initiating control connections to (${NODES[*]})..."
for node in ${NODES[*]}; do
    test -a socket.$node || ssh -Mf -S socket.$node $node "sleep 5m"
done

if [ $generate = yes ]; then
    echo "Generating unsorted input files..."
    c=0
    for node in ${NODES[*]}; do
	ssh -S socket.$node -T $node <<-EOF &
#    set -x
    start=0

    for p in $NUMA_PATHS; do
    	rm -f \$p/unsorted.* \$p/sorted.* \$p/$node.*.*

    	for (( i=0; i<$NUMA_CORES; i++ )); do
    		echo $node \$p/unsorted.\$i \$start 0x\`gensort -c -b\$start $maprecords \$p/unsorted.\$i 2>&1\` &
		let start+=$maprecords
    	done
    done

    wait
EOF
	let c+=maprecords*NUMA_CORES*numa_nodes
    done > checksums.txt

    wait
fi

if [ $tencentsort = yes ]; then
    echo "Cleaning caches..."
    for node in ${NODES[*]}; do
	ssh -S socket.$node -T $node <<-EOF &
    sync
    echo 3 >| /proc/sys/vm/drop_caches
EOF
    done

    wait

    echo "--- Tencent Sort! ---"
    echo "Range partitioning..."

    STARTTIME=$SECONDS
    $TIMESTAMP
    
    for node in ${NODES[*]}; do
	ssh -S socket.$node -T $node <<-EOF &
#    set -x
    pn=0
    for p in $NUMA_PATHS; do
    	for (( i=0; i<$NUMA_CORES; i++ )); do
	    $TIME numactl -N\$pn -m\$pn -- $PARTITION \$p/unsorted.\$i \$p/$node.\$i $mappers &
    	done

	let pn++
    done

    wait
EOF
    done

    wait

    $TIMESTAMP
    echo "Merge and sort..."
    c=0
    for node in ${NODES[*]}; do
	ssh -S socket.$node -T $node <<-EOF &
#    set -x
    pn=0
    c=$c
#    export PMEM_MOVNT_THRESHOLD=0
    for p in $NUMA_PATHS; do
    	for (( i=0; i<$NUMA_CORES; i++ )); do
	    $TIME numactl -N\$pn -m\$pn -- $QSORT \$p/sorted.\$c \$p/*.*.\$c &
	    let c++
    	done

	let pn++
    done

    wait
EOF
	let c+=NUMA_CORES*numa_nodes
    done

    wait

    $TIMESTAMP
    ENDTIME=$SECONDS
    python <<EOF
tb = ($RECORDS * $RECORDSIZE) / 1000000000.0
duration = $ENDTIME - $STARTTIME
print("Sorted %.2f GB in %u seconds => %.2f GB/minute, %.2f GB/s" % (tb, duration, (tb / duration) * 60, tb / duration))
EOF
    echo "---------------------"
fi

if [ $validate = yes ]; then
    echo "Validating sorted output..."
    for node in ${NODES[*]}; do
	ssh -S socket.$node -T $node <<-EOF &
#    set -x
    rm -f /tmp/minutesort.out.*.sum
    n=0
    for p in $NUMA_PATHS; do
    	for f in \$p/sorted.*; do
	    valsort -q -o /tmp/minutesort.out.\${f##\$p/sorted.}.sum \$f &
	    let n++
	done
    done

    wait
EOF
    done

    wait

    rm -f minutesort.out.*.sum
    for node in ${NODES[*]}; do
	scp -q $node:/tmp/minutesort.out.*.sum . &
    done

    wait

    cat `ls -v1 minutesort.out.*.sum` > all.sum
    valsort -s all.sum 2>validation.txt || true

    cat validation.txt

    CHECKSUMS=`awk '{ print $4 }' checksums.txt | paste -s -d+`
    CHECKSUM=`python -c "print hex($CHECKSUMS)"`
    VALSUM=0x`sed -ne 's/Checksum: \(.*\)/\1/p' validation.txt`

    if [ $CHECKSUM = $VALSUM ]; then
	echo "Checksum correct"
    else
	echo "CHECKSUM INCORRECT"
    fi
fi

echo "Closing control connections..."
for node in ${NODES[*]}; do
    ssh -q -S socket.$node -O exit $node
done
