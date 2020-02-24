#!/bin/bash
: <<'END'
This file is part of the Ristra portage project.
Please see the license file at the root of this repository, or at:
    https://github.com/laristra/portage/blob/master/LICENSE
END

set -e
set -x

${RUN_COMMAND} $TESTAPPDIR/swept_face_demo \
  --dim=2 \
  --ncells=10 \
  --remap_order=1 \
  --field="x+2y" \
  --ntimesteps=4 \
  --scale_by=10 \
  --output_meshes=false \
  --result_file="swept_dim_2_field_linear_order_1_vortex" \
  --keep_source=false \
  --simple=false
