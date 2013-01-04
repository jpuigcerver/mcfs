#!/bin/bash

DATASET_BINARIZE=$(dirname $0)/../dataset-binarize
LANG=C
awk -F:: '{printf("%d %d %f\n", $1-1, $2-1, $3)}' | \
$DATASET_BINARIZE -precision INT -minv 1 -maxv 5