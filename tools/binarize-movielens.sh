#!/bin/bash

awk -F:: '{print $1, $2, $3}' | \
../dataset-binarize -precision INT -minv 1 -maxv 5