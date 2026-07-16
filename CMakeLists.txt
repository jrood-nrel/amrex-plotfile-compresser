cmake_minimum_required(VERSION 3.20)

project(amrex-plotfile-compresser
    LANGUAGES CXX
    DESCRIPTION "Convert native AMReX plotfiles to HDF5 with ZFP compression"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Configurable options
# ---------------------------------------------------------------------------
set(AMReX_SPACEDIM "3" CACHE STRING "AMReX spatial dimension (1, 2, or 3)")

# ---------------------------------------------------------------------------
# AMReX submodule configuration
# Pull in AMReX from the git submodule before calling add_subdirectory so
# that our cache variables are visible when AMReX's CMakeLists.txt runs.
# ---------------------------------------------------------------------------
set(AMReX_MPI            ON  CACHE BOOL "Enable MPI"                FORCE)
set(AMReX_HDF5           ON  CACHE BOOL "Enable HDF5-based I/O"     FORCE)
set(AMReX_HDF5_ZFP       ON  CACHE BOOL "Enable ZFP compression in HDF5" FORCE)
set(AMReX_FORTRAN        OFF CACHE BOOL "Disable Fortran"           FORCE)
set(AMReX_LINEAR_SOLVERS OFF CACHE BOOL "Disable linear solvers"    FORCE)
set(AMReX_PARTICLES      OFF CACHE BOOL "Disable particle support"  FORCE)
set(AMReX_BUILD_TUTORIALS OFF CACHE BOOL "Disable tutorials"        FORCE)
set(AMReX_INSTALL        OFF CACHE BOOL "Disable AMReX install"     FORCE)

set(AMReX_SUBMODULE_DIR "${CMAKE_SOURCE_DIR}/submodules/amrex")
if(NOT EXISTS "${AMReX_SUBMODULE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "AMReX submodule not found at ${AMReX_SUBMODULE_DIR}.\n"
        "Run: git submodule update --init --recursive"
    )
endif()

add_subdirectory(${AMReX_SUBMODULE_DIR})

# ---------------------------------------------------------------------------
# Application target
# ---------------------------------------------------------------------------
add_executable(amrex-plotfile-compresser
    src/main.cpp
)

# AMReX exports a dimension-specific target; fall back to the generic one.
if(TARGET AMReX::amrex_${AMReX_SPACEDIM}d)
    target_link_libraries(amrex-plotfile-compresser PRIVATE AMReX::amrex_${AMReX_SPACEDIM}d)
elseif(TARGET AMReX::amrex)
    target_link_libraries(amrex-plotfile-compresser PRIVATE AMReX::amrex)
else()
    message(FATAL_ERROR "Could not find an AMReX CMake target to link against.")
endif()
