#[[
This file is part of the Ristra portage project.
Please see the license file at the root of this repository, or at:
    https://github.com/laristra/portage/blob/master/LICENSE
]]

project(portage)

cinch_minimum_required(2.0)



# If a C++14 compiler is available, then set the appropriate flags
include(cxx14)
check_for_cxx14_compiler(CXX14_COMPILER)
if(CXX14_COMPILER)
  enable_cxx14()
else()
  message(STATUS "C++14 compatible compiler not found")
endif()

# If we couldn't find a C++14 compiler, try to see if a C++11 
# compiler is available, then set the appropriate flags
if (NOT CXX14_COMPILER)
  include(cxx11)
  check_for_cxx11_compiler(CXX11_COMPILER)
  if(CXX11_COMPILER)
    enable_cxx11()
  else()
    message(FATAL_ERROR "C++11 compatible compiler not found")
  endif()
endif()



# cinch extras

cinch_load_extras()

set(CINCH_HEADER_SUFFIXES "\\.h")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")


#-----------------------------------------------------------------------------
# Gather all the third party libraries needed for Portage
#-----------------------------------------------------------------------------

set(portage_LIBRARIES)
#------------------------------------------------------------------------------#
# Set up MPI builds
# (eventually most of this should be pushed down into cinch)
#------------------------------------------------------------------------------#
set(ENABLE_MPI OFF CACHE BOOL "")
if (ENABLE_MPI)
  find_package(MPI REQUIRED)
  add_definitions(-DENABLE_MPI)
# TODO:  Modify the below to use wrapper compilers instead of flags
#        (there isn't an obvious good way to do this)
#  add_definitions(${MPI_CXX_COMPILE_FLAGS})
#  include_directories(${MPI_CXX_INCLUDE_PATH})
#  link_directories(${MPI_CXX_LIBRARY_DIRS})
endif ()


set(ARCHOS ${CMAKE_SYSTEM_PROCESSOR}_${CMAKE_SYSTEM_NAME})

#-----------------------------------------------------------------------------
# Wonton
#-----------------------------------------------------------------------------
if (WONTON_DIR)
 # Link with an existing installation of Wonton, if provided. 
 find_package(WONTON REQUIRED)
 message(STATUS "WONTON_LIBRARIES=${WONTON_LIBRARIES}" )
 include_directories(${WONTON_INCLUDE_DIR})
 message(STATUS "WONTON_INCLUDE_DIRS=${WONTON_INCLUDE_DIR}")
else (WONTON_DIR)
 # Build Wonton from a submodule
 file(GLOB _wonton_contents ${CMAKE_SOURCE_DIR}/wonton/*)
 if (_wonton_contents)
   if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
     # We are building portage, and wonton is a subdirectory
     add_subdirectory(${CMAKE_SOURCE_DIR}/wonton)
   endif()
   include_directories(${CMAKE_SOURCE_DIR}/wonton)
   list(APPEND portage_LIBRARIES wonton)
   set(WONTON_FOUND TRUE)
 else()
   set(WONTON_FOUND FALSE)
 endif(_wonton_contents)
endif (WONTON_DIR)

if (NOT WONTON_FOUND)
   message(FATAL_ERROR "WONTON_DIR is not specified and Wonton is not a subdirectory !")
endif() 

#-----------------------------------------------------------------------------
# FleCSI and FleCSI-SP location
#-----------------------------------------------------------------------------

set(ENABLE_FleCSI FALSE CACHE BOOL "Use FleCSI")
if (ENABLE_FleCSI)
 
 find_package(FleCSI REQUIRED)
 message(STATUS "FleCSI_LIBRARIES=${FleCSI_LIBRARIES}" )
 include_directories(${FleCSI_INCLUDE_DIR})
 message(STATUS "FleCSI_INCLUDE_DIRS=${FleCSI_INCLUDE_DIR}")

 find_package(FleCSISP REQUIRED)
 message(STATUS "FleCSISP_LIBRARIES=${FleCSISP_LIBRARIES}" )
 include_directories(${FleCSISP_INCLUDE_DIR})
 message(STATUS "FleCSISP_INCLUDE_DIRS=${FleCSISP_INCLUDE_DIR}")

  ######################################################################
  # This is a placeholder for how we would do IO with FleCSI
  # There are still some issues with dumping the targetMesh data
  #
  # WARNING!!! THIS IS POTENTIALLY FRAGILE
  # it appears to work, but could cause conflicts with EXODUS and
  # other libraries used by Jali
  #
  # FOR NOW THIS IS DISABLED UNTIL WE CAN GET A PROPER WORKAROUND
  ######################################################################
  # STRING(REPLACE "flecsi" "flecsi-tpl" FLECSI_TPL_DIR ${FLECSI_INSTALL_DIR})
  # message(STATUS "FLECSI_TPL_DIR=${FLECSI_TPL_DIR}")
  # if(IS_DIRECTORY ${FLECSI_TPL_DIR})
  #   find_library(EXODUS_LIBRARY
  #     NAMES exodus
  #     PATHS ${FLECSI_TPL_DIR}
  #     PATH_SUFFIXES lib
  #     NO_DEFAULT_PATH)
  #   find_path(EXODUS_INCLUDE_DIR
  #     NAMES exodusII.h
  #     PATHS ${FLECSI_TPL_DIR}
  #     PATH_SUFFIXES include
  #     NO_DEFAULT_PATH)

  #   if(EXODUS_LIBRARY AND EXODUS_INCLUDE_DIR)
  #     set(FLECSI_LIBRARIES ${EXODUS_LIBRARY} ${FLECSI_LIBRARIES})
  #     include_directories(${EXODUS_INCLUDE_DIR})
  #     add_definitions(-DHAVE_EXODUS)
  #   endif(EXODUS_LIBRARY AND EXODUS_INCLUDE_DIR)

  # endif(IS_DIRECTORY ${FLECSI_TPL_DIR})
endif()


#------------------------------------------------------------------------------#
# Configure Jali
# (this includes the TPLs that Jali will need)
#------------------------------------------------------------------------------#

if (Jali_DIR)

   # Look for the Jali package

   find_package(Jali REQUIRED
                HINTS ${Jali_DIR}/lib)

   message(STATUS "Located Jali")
   message(STATUS "Jali_DIR=${Jali_DIR}")

   # add full path to jali libs
   unset(_LIBS)
   foreach (_lib ${Jali_LIBRARIES})
      set(_LIBS ${_LIBS};${Jali_LIBRARY_DIRS}/lib${_lib}.a)
   endforeach()
   set(Jali_LIBRARIES ${_LIBS})

   include_directories(${Jali_INCLUDE_DIRS} ${Jali_TPL_INCLUDE_DIRS})

endif (Jali_DIR)

#------------------------------------------------------------------------------#
# Configure LAPACKE
#------------------------------------------------------------------------------#

if (LAPACKE_DIR)

  # Directly look for cmake config file in LAPACKE_DIR and below
  file(GLOB_RECURSE LAPACKE_CONFIG_FILE ${LAPACKE_DIR}/lapacke-config.cmake)

  if (NOT LAPACKE_CONFIG_FILE)
    message(FATAL_ERROR " LAPACKE CMAKE config file not found under LAPACKE_DIR (${LAPACKE_DIR})")
  endif (NOT LAPACKE_CONFIG_FILE)

  message(STATUS "LAPACKE_CONFIG_FILE ${LAPACKE_CONFIG_FILE}")

  get_filename_component(LAPACKE_CONFIG_PATH ${LAPACKE_CONFIG_FILE} DIRECTORY)
  message(status " LAPACKE_CONFIG_PATH ${LAPACKE_CONFIG_PATH}")

  # If successful, the config file will set LAPACKE_LIBRARIES,
  # LAPACKE_lapack_LIBRARIES and LAPACKE_blas_LIBRARIES

  find_package(LAPACKE NO_MODULE NO_DEFAULT_PATH HINTS ${LAPACKE_CONFIG_PATH})

  if (LAPACKE_LIBRARIES STREQUAL "lapacke")

    # LAPACKE config file does not set the library path but it does set the
    # LAPACKE_INCLUDE_DIRS path. Try to back out the library path using this
    # and the top level directory as starting points for a find_library command

    find_library(LAPACKE_LIBRARY NAMES lapacke
                 NO_CMAKE_SYSTEM_PATH NO_DEFAULT_PATH
                 HINTS ${LAPACKE_DIR} ${LAPACKE_INCLUDE_DIRS}/..
	         PATH_SUFFIXES lib lib64)
	       

    # Extract path of directory in which library files live to pass as a lib
    # search directory for the linker to find lapacke, lapack and blas libs

    get_filename_component(LAPACKE_LIBRARY_DIR ${LAPACKE_LIBRARY} DIRECTORY)

    set(LAPACKE_LIBRARIES "-Wl,-rpath,${LAPACKE_LIBRARY_DIR} -L${LAPACKE_LIBRARY_DIR} -l${LAPACKE_LIBRARIES} -l${LAPACK_lapack_LIBRARIES} -l${LAPACK_blas_LIBRARIES}")

    # If we don't want to link with Fortran then we have to tell it to link
    # with the Fortran libraries because LAPACK is written/compiled in Fortran
    #
    # NEEDED FOR STATIC LAPACK LIBS

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
      set(LAPACKE_LIBRARIES "${LAPACKE_LIBRARIES} -lgfortran")    
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
      set(LAPACKE_LIBRARIES "${LAPACKE_LIBRARIES} -lifcore")    
    endif()

  endif(LAPACKE_LIBRARIES STREQUAL "lapacke")

else (LAPACKE_DIR)

  # Use FindLAPACKE.cmake provided by cinch or cmake to find it
  # FindLAPACKE.cmake provided by cinch requires PC_LAPACKE_INCLUDE_DIRS and
  # PC_LAPACKE_LIBRARY to be able to find LAPACKE

  find_package(LAPACKE)

endif (LAPACKE_DIR)

if (LAPACKE_FOUND) 
  include_directories(${LAPACKE_INCLUDE_DIRS})
  add_definitions("-DHAVE_LAPACKE")

  message(STATUS "LAPACKE_FOUND ${LAPACKE_FOUND}")
  message(STATUS "LAPACKE_LIBRARIES  ${LAPACKE_LIBRARIES}")
else (LAPACKE_FOUND)
   unset(LAPACKE_LIBRARIES)  # otherwise it will be LAPACKE-NOTFOUND or something
endif (LAPACKE_FOUND)


#------------------------------------------------------------------------------#
# Find the NANOFLANN (https://github.com/jlblancoc/nanoflann) package
# Nanoflann is a header only package
#------------------------------------------------------------------------------#

if (NANOFLANN_DIR)
    find_path(nanoflann_FOUND
              nanoflann.hpp
              HINTS ${NANOFLANN_DIR}/include)
    if (nanoflann_FOUND)
      include_directories(${NANOFLANN_DIR}/include)
      add_definitions("-DHAVE_NANOFLANN")
    endif (nanoflann_FOUND)
endif (NANOFLANN_DIR)

#------------------------------------------------------------------------------#
# Find Tangram (includes only package)
#------------------------------------------------------------------------------#

if (TANGRAM_DIR)
  find_path(TANGRAM
  	    REQUIRED
            tangram/support/tangram.h
            HINTS ${TANGRAM_DIR}/include)
  message(STATUS "TANGRAM FOUND? ${TANGRAM}")
  if (TANGRAM)
    set(TANGRAM_FOUND ON)
    set(TANGRAM_INCLUDE_DIRS ${TANGRAM_DIR}/include)
    include_directories(${TANGRAM_INCLUDE_DIRS})
    add_definitions("-DHAVE_TANGRAM")
  endif (TANGRAM)
else (TANGRAM_DIR)
  message(STATUS "TANGRAM_DIR not specified. Restricted to single material remap")
endif (TANGRAM_DIR)

#------------------------------------------------------------------------------#
# Find XMOF2D
#------------------------------------------------------------------------------#

if (TANGRAM_FOUND)
  find_package(XMOF2D
    HINTS ${XMOF2D_DIR})
  if (XMOF2D_FOUND)
    include_directories(${XMOF2D_INCLUDE_DIRS})

# What XMOF2D puts as XMOF2D_LIBRARIES is not a complete path but just a name
# Discover the library and cat it with the library dir to make XMOF2D_LIBRARIES
    find_library(XMOF2D_LIBRARY
      NAMES ${XMOF2D_LIBRARY_NAME}
      HINTS ${XMOF2D_LIBRARY_DIR})
    if (XMOF2D_LIBRARY)
      set(XMOF2D_LIBRARIES ${XMOF2D_LIBRARY})
    endif (XMOF2D_LIBRARY)
    message(STATUS "XMOF2D LIBRARIES ---> ${XMOF2D_LIBRARIES}")
    add_definitions("-DHAVE_XMOF2D")
  endif (XMOF2D_FOUND)
endif (TANGRAM_FOUND)

#-----------------------------------------------------------------------------
# General NGC include directory information
#-----------------------------------------------------------------------------
set(NGC_INCLUDE_DIR "$ENV{NGC_INCLUDE_DIR}" CACHE PATH "NGC include directory")
if(NGC_INCLUDE_DIR)
  message(STATUS "Using NGC_INCLUDE_DIR=${NGC_INCLUDE_DIR}")
endif(NGC_INCLUDE_DIR)

#-----------------------------------------------------------------------------
# Thrust information
#-----------------------------------------------------------------------------
set(ENABLE_THRUST FALSE CACHE BOOL "Use Thrust")
if(ENABLE_THRUST)
  message(STATUS "Enabling compilation with Thrust")
  # allow the user to specify a THRUST_DIR, otherwise use ${NGC_INCLUDE_DIR}
  # NOTE: thrust internally uses include paths from the 'root' directory, e.g.
  #
  #       #include "thrust/device_vector.h"
  #
  #       so the path here should point to the directory that has thrust as
  #       a subdirectory.
  # Use THRUST_DIR directly if specified, otherwise try to build from NGC
  set(THRUST_DIR "${NGC_INCLUDE_DIR}" CACHE PATH "Thrust directory")
  message(STATUS "Using THRUST_DIR=${THRUST_DIR}")

  # Allow for swapping backends
  set(THRUST_BACKEND "THRUST_DEVICE_SYSTEM_OMP" CACHE STRING "Thrust backend")
  message(STATUS "Using ${THRUST_BACKEND} as Thrust backend.")
  include_directories(${THRUST_DIR})
  add_definitions(-DTHRUST)
  add_definitions(-DTHRUST_DEVICE_SYSTEM=${THRUST_BACKEND})

  if("${THRUST_BACKEND}" STREQUAL "THRUST_DEVICE_SYSTEM_OMP")
    FIND_PACKAGE( OpenMP REQUIRED)
    if(OPENMP_FOUND)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    endif(OPENMP_FOUND)
  endif ()

  if("${THRUST_BACKEND}" STREQUAL "THRUST_DEVICE_SYSTEM_TBB")
    FIND_PACKAGE(TBB REQUIRED)
    if(TBB_FOUND)
      include_directories(${TBB_INCLUDE_DIRS})
      link_directories(${TBB_LIBRARY_DIRS})
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -ltbb")
    endif(TBB_FOUND)
  endif()

endif(ENABLE_THRUST)


#-----------------------------------------------------------------------------
# Find Boost
#-----------------------------------------------------------------------------
if(NOT ENABLE_THRUST)
  find_package(Boost REQUIRED)
  if(Boost_FOUND)
    message(STATUS "Boost location: ${Boost_INCLUDE_DIRS}")
    include_directories( ${Boost_INCLUDE_DIRS} )
  endif(Boost_FOUND)
endif(NOT ENABLE_THRUST)




#-----------------------------------------------------------------------------
# Now add the source directories and library targets
#-----------------------------------------------------------------------------

cinch_add_application_directory(app)
cinch_add_library_target(portage portage)
# TODO - merge LAPACKE_LIBRARIES into portage_LIBRARIES
cinch_target_link_libraries(portage ${portage_LIBRARIES})
cinch_target_link_libraries(portage ${LAPACKE_LIBRARIES})
cinch_target_link_libraries(portage ${WONTON_LIBRARIES})


# Add application tests
# May pull this logic into cinch at some future point
option(ENABLE_APP_TESTS "Enable testing of full app" OFF)
if(ENABLE_APP_TESTS)
  enable_testing()
endif()


