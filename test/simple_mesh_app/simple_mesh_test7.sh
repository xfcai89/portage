#!/bin/bash

# Exit on error
set -e
# Echo each command
set -x

# 3d 2nd order node-centered remap of linear func
mpirun -np 1 $APPDIR/simple_mesh 7 4 5

# Compare the values for the field
$APPDIR/apptest_cmp field_gold7.txt field7.txt 1e-12
