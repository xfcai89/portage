#[[
This file is part of the Ristra portage project.
Please see the license file at the root of this repository, or at:
    https://github.com/laristra/portage/blob/master/LICENSE
]]
#-----------------------------------------------------------------------------~#

# Add header files
set(intersect_HEADERS
        clipper.hpp
        intersect_clipper.h
        intersect_boxes.h
        intersect_polys_r2d.h
        intersect_r2d.h
        intersect_polys_r3d.h
        intersect_r3d.h
        intersect_rNd.h
        intersect_swept_face.h
        dummy_interface_reconstructor.h
        PARENT_SCOPE
        )

# Add source files
set(intersect_SOURCES clipper.cpp PARENT_SCOPE)

# Add unit test
if (ENABLE_UNIT_TESTS)
    if (Jali_DIR)
        cinch_add_unit(test_intersect_clipper
                SOURCES test/test_intersect_clipper.cc clipper.cpp
                LIBRARIES ${Jali_LIBRARIES} ${Jali_TPL_LIBRARIES}
                POLICY MPI)

        cinch_add_unit(test_intersect_swept_face_2d
                SOURCES test/test_intersect_swept_face_2D.cc
                LIBRARIES portage ${Jali_LIBRARIES} ${Jali_TPL_LIBRARIES}
                POLICY MPI)

        cinch_add_unit(test_intersect_swept_face_3d
                SOURCES test/test_intersect_swept_face_3D.cc
                LIBRARIES portage ${Jali_LIBRARIES} ${Jali_TPL_LIBRARIES}
                POLICY MPI)
    endif (Jali_DIR)

    cinch_add_unit(test_intersect_boxes
            SOURCES test/test_intersect_boxes.cc
            LIBRARIES wonton
            POLICY SERIAL)

    cinch_add_unit(test_intersect_r2d
            SOURCES test/test_intersect_r2d.cc
            LIBRARIES wonton
            POLICY SERIAL)

    cinch_add_unit(test_intersect_r3d
            SOURCES test/test_intersect_r3d.cc
            LIBRARIES portage wonton
            POLICY SERIAL)

    if (TANGRAM_FOUND AND XMOF2D_FOUND)
        include_directories(${TANGRAM_INCLUDE_DIRS})
        cinch_add_unit(test_intersect_tangram_2d
                SOURCES test/test_tangram_intersect_2D
                LIBRARIES portage
                POLICY SERIAL)

        cinch_add_unit(test_intersect_tangram_3d
                SOURCES test/test_tangram_intersect_3D
                LIBRARIES portage
                POLICY SERIAL)
    endif (TANGRAM_FOUND AND XMOF2D_FOUND)
endif (ENABLE_UNIT_TESTS)

