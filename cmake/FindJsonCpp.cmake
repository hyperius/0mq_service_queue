# - Try to find libzmq
# Once done, this will define
#
#  ZeroMQ_FOUND - system has libzmq
#  ZeroMQ_INCLUDE_DIRS - the libzmq include directories
#  ZeroMQ_LIBRARIES - link these to use libzmq

include(LibFindMacros)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(JsonCpp_PKGCONF jsoncpp)

# Include dir
find_path(JsonCpp_INCLUDE_DIR
  NAMES json/json.h
  PATHS ${JSONCPP_ROOT}/include ${JsonCpp_PKGCONF_INCLUDE_DIRS}
)

# Finally the library itself
find_library(JsonCpp_LIBRARY
  NAMES jsoncpp
  PATHS ${JSONCPP_ROOT}/lib ${JsonCpp_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(JsonCpp_PROCESS_INCLUDES JsonCpp_INCLUDE_DIR JsonCpp_INCLUDE_DIRS)
set(JsonCpp_PROCESS_LIBS JsonCpp_LIBRARY JsonCpp_LIBRARIES)
libfind_process(JsonCpp)