#!/bin/bash

function rand() {
    od -An -N4 -t u4 -i /dev/urandom | head -n 1 | awk '{print $1}'
}

if [ $# -ne 1 ]; then
    echo "Usage: $0 dataset"
    exit 1
fi

DATASET_INFO=$(dirname $0)/../dataset-info
DATASET_PARTITION=$(dirname $0)/../dataset-partition
MCFS_TRAIN=$(dirname $0)/../mcfs-train

SIMILARITIES=(COSINE COSINE_SQRT COSINE_POW2 COSINE_EXPO \
INV_NORM_P1 INV_NORM_P2 INV_NORM_PI I_N_P1_EXPO I_N_P2_EXPO I_N_PI_EXPO)
MIN_K=1
MAX_K=$($DATASET_INFO -input $1 | grep 'Users:' | awk '{print $2}')
INC_K=1
REPS=5

# Explore hyperparameters
echo "# K ${SIMILARITIES[@]}"
k=$MIN_K
while [ $k -le $MAX_K ]; do
    echo -n "$k"
    for sim in ${SIMILARITIES[@]}; do
	r=1
	sum_rmse=0
	while [ $r -le $REPS ]; do
	    seed=$(rand)
	    train=/tmp/neighbours-$(basename $1)-k${k}-f${sim}-r${r}-s${seed}-train
	    valid=/tmp/neighbours-$(basename $1)-k${k}-f${sim}-r${r}-s${seed}-valid
	    log=/tmp/neighbours-$(basename $1)-k${k}-f${sim}-r${r}-s${seed}-log
	    $DATASET_PARTITION -input $1 -part1 $train -part2 $valid -seed $seed
	    out=$($MCFS_TRAIN -mconf "k: $k similarity: $sim" -mtype neighbours \
		-seed $seed -train $train -valid $valid --logtostderr 2> $log)
	    if [ $? -ne 0 ]; then
		echo "Failed k=$k f=$sim r=$r s=$seed..."
		exit 1
	    fi
	    rmse=$(echo $out | awk '{print $3}')
	    sum_rmse=$(echo "$rmse + $sum_rmse" | bc -l)
	    r=$[r + 1]
	done
	avg_rmse=$(echo "$sum_rmse / $REPS" | bc -l)
	echo -n " $avg_rmse"
    done
    echo ""
    k=$[k + INC_K]
done
