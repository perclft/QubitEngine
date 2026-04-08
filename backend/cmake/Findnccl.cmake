# Find NCCL
#
# Find the NCCL (NVIDIA Collective Communications Library) headers and libraries.
#
# The following variables are set:
#  NCCL_FOUND        - True if NCCL is found
#  NCCL_INCLUDE_DIRS - Directory where nccl.h is located
#  NCCL_LIBRARIES    - The NCCL libraries
#  nccl::nccl        - Imported target for NCCL
#
# The following search paths are used:
#  NCCL_ROOT_DIR      - User-defined root directory for NCCL
#  CUDA_TOOLKIT_ROOT_DIR - CUDA toolkit directory

find_path(NCCL_INCLUDE_DIR
    NAMES nccl.h
    HINTS ${NCCL_ROOT_DIR} ${CUDA_TOOLKIT_ROOT_DIR}
    PATH_SUFFIXES include
)

find_library(NCCL_LIBRARY
    NAMES nccl
    HINTS ${NCCL_ROOT_DIR} ${CUDA_TOOLKIT_ROOT_DIR}
    PATH_SUFFIXES lib lib64 lib/x86_64-linux-gnu
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(nccl
    REQUIRED_VARS NCCL_LIBRARY NCCL_INCLUDE_DIR
)

if(NCCL_FOUND)
    set(NCCL_INCLUDE_DIRS ${NCCL_INCLUDE_DIR})
    set(NCCL_LIBRARIES ${NCCL_LIBRARY})
    
    if(NOT TARGET nccl::nccl)
        add_library(nccl::nccl UNKNOWN IMPORTED)
        set_target_properties(nccl::nccl PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${NCCL_INCLUDE_DIRS}"
            IMPORTED_LOCATION "${NCCL_LIBRARIES}"
        )
    endif()
endif()

mark_as_advanced(NCCL_INCLUDE_DIR NCCL_LIBRARY)
