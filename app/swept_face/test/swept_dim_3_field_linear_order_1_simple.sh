#!/bin/bash
: <<'END'
This file is part of the Ristra portage project.
Please see the license file at the root of this repository, or at:
    https://github.com/laristra/portage/blob/master/LICENSE
END

set -e
set -x

epsilon=1.e-9

# run test
${RUN_COMMAND} $TESTAPPDIR/swept_face_demo \
  --dim=3 \
  --ncells=10 \
  --remap_order=1 \
  --field="x+2y+3z" \
  --ntimesteps=4 \
  --scale_by=10 \
  --output_meshes=false \
  --result_file="data_dim_3_field_linear_order_1_simple" \
  --keep_source=false \
  --simple=true

# compare values with related gold file
$CMPAPPDIR/apptest_cmp \
  "gold_dim_3_field_linear_order_1_simple.txt" \
  "data_dim_3_field_linear_order_1_simple.txt" \
  ${epsilon}