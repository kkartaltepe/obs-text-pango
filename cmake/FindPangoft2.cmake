find_package(PkgConfig)
pkg_check_modules(PC_PANGOFT2 pangoft2 QUIET)

FIND_PATH(PANGOFT2_INCLUDE_DIR
    NAMES
        pango/pangoft2.h
    HINTS
        ${PC_PANGOFT2_INCLUDEDIR}
        ${PC_PANGOFT2_INCLUDE_DIRS}
)

FIND_LIBRARY(PANGOFT2_LIBRARY
    NAMES
        pangoft2 libpangoft2 pangoft2-1.0
    HINTS
        ${PC_PANGOFT2_LIBDIR}
    PATH_SUFFIXES
        pango
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Pangoft2 DEFAULT_MSG
    PANGOFT2_LIBRARY PANGOFT2_INCLUDE_DIR
)

MARK_AS_ADVANCED(PANGOFT2_INCLUDE_DIR PANGOFT2_LIBRARY)
set(PANGOFT2_INCLUDE_DIRS ${PANGOFT2_INCLUDE_DIR})
set(PANGOFT2_LIBRARIES ${PANGOFT2_LIBRARY})
