#[[
This file is part of the Ristra portage project.
Please see the license file at the root of this repository, or at:
    https://github.com/laristra/portage/blob/master/LICENSE
]]
#-----------------------------------------------------------------------------~#


#-----------------------------------------------------------------------------~#

set(distributed_HEADERS
    mpi_bounding_boxes.h
    mpi_particle_distribute.h
    PARENT_SCOPE)

if (ENABLE_UNIT_TESTS)
  if (Jali_DIR)
    cinch_add_unit(test_mpi_bounding_boxes
                   SOURCES test/test_mpi_bounding_boxes.cc
                   LIBRARIES portage  ${Jali_LIBRARIES} ${Jali_TPL_LIBRARIES}
                   POLICY MPI
                   THREADS 4)

    cinch_add_unit(test_mpi_particle_distribute
                   SOURCES test/test_mpi_particle_distribute.cc
                   LIBRARIES portage  ${Jali_LIBRARIES} ${Jali_TPL_LIBRARIES}
                   POLICY MPI
                   THREADS 4)
  endif (Jali_DIR)
endif(ENABLE_UNIT_TESTS)

