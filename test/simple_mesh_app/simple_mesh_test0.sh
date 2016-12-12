#!/bin/bash

# Exit on error
set -e
# Echo each command
set -x

# 3d 1st order cell-centered remap of linear func
mpirun -np 1 ${APPDIR}/simple_mesh 0 4 5

# Compare the values for the field
${APPDIR}/apptest_cmp field_gold0.txt field0.txt 5e-12
