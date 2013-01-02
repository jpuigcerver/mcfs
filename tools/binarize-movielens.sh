#!/bin/bash

DATASET_BINARIZE=$(dirname $0)/../dataset-binarize
awk -F:: '{print $1, $2, $3}' | \
$DATASET_BINARIZE -precision INT -minv 1 -maxv 5