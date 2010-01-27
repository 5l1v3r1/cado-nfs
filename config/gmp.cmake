
# You can force a path to gmp.h using the environment variables GMP, or
# GMP_INCDIR and GMP_LIBDIR
string(COMPARE NOTEQUAL "$ENV{GMP}" "" HAS_GMP_OVERRIDE)
if (HAS_GMP_OVERRIDE)
    message(STATUS "Adding $ENV{GMP} to the search path for Gnu MP")
    set(GMP_INCDIR_HINTS "$ENV{GMP}/include" ${GMP_INCDIR_HINTS})
    set(GMP_INCDIR_HINTS "$ENV{GMP}"         ${GMP_INCDIR_HINTS})
    set(GMP_LIBDIR_HINTS "$ENV{GMP}/lib"     ${GMP_LIBDIR_HINTS})
    set(GMP_LIBDIR_HINTS "$ENV{GMP}/.libs"   ${GMP_LIBDIR_HINTS})
endif(HAS_GMP_OVERRIDE)
string(COMPARE NOTEQUAL "$ENV{GMP_INCDIR}" "" HAS_GMP_INCDIR_OVERRIDE)
if (HAS_GMP_INCDIR_OVERRIDE)
    message(STATUS "Adding $ENV{GMP_INCDIR} to the search path for Gnu MP")
    set(GMP_INCDIR_HINTS "$ENV{GMP_INCDIR}" ${GMP_INCDIR_HINTS})
endif(HAS_GMP_INCDIR_OVERRIDE)
string(COMPARE NOTEQUAL "$ENV{GMP_LIBDIR}" "" HAS_GMP_LIBDIR_OVERRIDE)
if (HAS_GMP_LIBDIR_OVERRIDE)
    message(STATUS "Adding $ENV{GMP_LIBDIR} to the search path for Gnu MP")
    set(GMP_LIBDIR_HINTS "$ENV{GMP_LIBDIR}"     ${GMP_LIBDIR_HINTS})
endif(HAS_GMP_LIBDIR_OVERRIDE)
find_path   (GMP_INCDIR gmp.h HINTS ${GMP_INCDIR_HINTS} DOC "Gnu MP headers")
find_library(GMP_LIB    gmp   HINTS ${GMP_LIBDIR_HINTS} DOC "Gnu MP library")
# Yeah. CMake docs defines the ``PATH'' to a file as being its dirname. Very
# helpful documentation there :-((
get_filename_component(GMP_LIBDIR ${GMP_LIB} PATH)
message(STATUS "GMP_INCDIR=${GMP_INCDIR}")
message(STATUS "GMP_LIBDIR=${GMP_LIBDIR}")
string(COMPARE NOTEQUAL "${GMP_INCDIR}" GMP_INCDIR-NOTFOUND GMP_INCDIR_OK)
string(COMPARE NOTEQUAL "${GMP_LIBDIR}" GMP_LIBDIR-NOTFOUND GMP_LIBDIR_OK)
if(GMP_INCDIR_OK)
include_directories(${GMP_INCDIR})
endif(GMP_INCDIR_OK)
if(GMP_LIBDIR_OK)
link_directories(${GMP_LIBDIR})
endif(GMP_LIBDIR_OK)
