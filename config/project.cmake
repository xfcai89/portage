#~----------------------------------------------------------------------------~#
# Copyright (c) 2014 Los Alamos National Security, LLC
# All rights reserved.
#~----------------------------------------------------------------------------~#

project(portage)

cinch_add_application_directory(app)
cinch_add_library_target(portage src)

# Create some subcomponent libraries for the purposes of unit testing
#
# NOT ABLE TO DO EITHER OF THESE

if (ENABLE_UNIT_TESTS)
#  cinch_add_library_target(search src/search)
#  cinch_add_library_target(intersect src/intersect)
#  cinch_add_library_target(remap src/remap)
#  cinch_add_library_target(state src/state)

#  cinch_add_library_target(portage_search src/search)
#  cinch_add_library_target(portage_intersect src/intersect)
#  cinch_add_library_target(portage_remap src/remap)
#  cinch_add_library_target(portage_state src/state)
endif()

# Add application tests
# May pull this logic into cinch at some future point
option(ENABLE_APP_TESTS "Enable testing of full app" OFF)
if(ENABLE_APP_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

#------------------------------------------------------------------------------#
# 
#------------------------------------------------------------------------------#

set(CINCH_HEADER_SUFFIXES "\\.h")

