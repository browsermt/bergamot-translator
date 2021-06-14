##
# This CMake modules sets the project version from a version file.
#
# The module sets the following variables:
#
# * PROJECT_VERSION_STRING
# * PROJECT_VERSION_STRING_FULL
# * PROJECT_VERSION_MAJOR
# * PROJECT_VERSION_MINOR
# * PROJECT_VERSION_PATCH
# * PROJECT_VERSION_TWEAK
# * PROJECT_VERSION_GIT_SHA
#
# This module is public domain, use it as it fits you best.
##

# Get full string version from file
if(PROJECT_VERSION_FILE)
  file(STRINGS ${PROJECT_VERSION_FILE} PROJECT_VERSION_STRING)
else()
  file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/BERGAMOT_VERSION PROJECT_VERSION_STRING)
endif()

# Get current commit SHA from git
execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE PROJECT_VERSION_GIT_SHA
  OUTPUT_STRIP_TRAILING_WHITESPACE)

# Get partial versions into a list
string(REGEX MATCHALL "-.*$|[0-9]+" PROJECT_PARTIAL_VERSION_LIST
  ${PROJECT_VERSION_STRING})

# Set the version numbers
list(GET PROJECT_PARTIAL_VERSION_LIST 0 PROJECT_VERSION_MAJOR)
list(GET PROJECT_PARTIAL_VERSION_LIST 1 PROJECT_VERSION_MINOR)
list(GET PROJECT_PARTIAL_VERSION_LIST 2 PROJECT_VERSION_PATCH)

# The tweak part is optional, so check if the list contains it
list(LENGTH PROJECT_PARTIAL_VERSION_LIST PROJECT_PARTIAL_VERSION_LIST_LEN)
if(PROJECT_PARTIAL_VERSION_LIST_LEN GREATER 3)
  list(GET PROJECT_PARTIAL_VERSION_LIST 3 PROJECT_VERSION_TWEAK)
  string(SUBSTRING ${PROJECT_VERSION_TWEAK} 1 -1 PROJECT_VERSION_TWEAK)
endif()

# Unset the list
unset(PROJECT_PARTIAL_VERSION_LIST)

# Set full project version string
set(PROJECT_VERSION_STRING_FULL
  ${PROJECT_VERSION_STRING}+${PROJECT_VERSION_GIT_SHA})

# Print all variables for debugging
#message(STATUS ${PROJECT_VERSION_STRING_FULL})
#message(STATUS ${PROJECT_VERSION_STRING})
#message(STATUS ${PROJECT_VERSION_MAJOR})
#message(STATUS ${PROJECT_VERSION_MINOR})
#message(STATUS ${PROJECT_VERSION_PATCH})
#message(STATUS ${PROJECT_VERSION_TWEAK})
#message(STATUS ${PROJECT_VERSION_GIT_SHA})
