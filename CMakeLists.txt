cmake_minimum_required(VERSION 3.20)

project(Cost_decomposition LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------
# CPLEX Studio location: a CACHE variable (overridable with
# -DCPLEX_STUDIO_DIR=/path/to/CPLEX_Studio... on the cmake command line,
# or the CPLEX_STUDIO_DIR environment variable) so the SAME CMakeLists.txt
# builds unchanged on both the Windows development machine and a Linux
# cluster -- see experiments/README.md for the exact Linux configure
# command. The Windows default below is preserved exactly for the existing
# MSVC build; on any other platform CPLEX_STUDIO_DIR MUST be supplied
# (no cluster-specific path is guessed here).
# ----------------------------------------------------------------------
if(DEFINED ENV{CPLEX_STUDIO_DIR} AND NOT DEFINED CPLEX_STUDIO_DIR)
    set(CPLEX_STUDIO_DIR "$ENV{CPLEX_STUDIO_DIR}" CACHE PATH "IBM ILOG CPLEX Studio installation directory")
elseif(WIN32)
    set(CPLEX_STUDIO_DIR "C:/Program Files/IBM/ILOG/CPLEX_Studio201" CACHE PATH "IBM ILOG CPLEX Studio installation directory")
else()
    set(CPLEX_STUDIO_DIR "" CACHE PATH "IBM ILOG CPLEX Studio installation directory")
endif()

if(NOT WIN32 AND CPLEX_STUDIO_DIR STREQUAL "")
    message(FATAL_ERROR
        "CPLEX_STUDIO_DIR is not set. Configure with "
        "-DCPLEX_STUDIO_DIR=/path/to/CPLEX_Studio<version> or export the "
        "CPLEX_STUDIO_DIR environment variable (e.g. via your cluster's "
        "CPLEX/module setup) before running cmake. See experiments/README.md.")
endif()

add_executable(drcrffsp
    src/CP_Model.cpp
    src/Decomposition.cpp
    src/DRCRFFSP_Instance.cpp
    src/ExperimentRunner.cpp
    src/Job.cpp
    src/Machine.cpp
    src/main.cpp
    src/Masterproblem.cpp
    src/Operation.cpp
    src/Stage.cpp
    src/Worker.cpp
)

target_include_directories(drcrffsp PRIVATE
    include
    "${CPLEX_STUDIO_DIR}/concert/include"
    "${CPLEX_STUDIO_DIR}/cpoptimizer/include"
    "${CPLEX_STUDIO_DIR}/cplex/include"
)

target_compile_definitions(drcrffsp PRIVATE
    IL_STD
)

if(WIN32)

    target_link_libraries(drcrffsp PRIVATE
        "$<$<CONFIG:Debug>:${CPLEX_STUDIO_DIR}/cpoptimizer/lib/x64_windows_msvc14/stat_mdd/cp.lib>"
        "$<$<CONFIG:Debug>:${CPLEX_STUDIO_DIR}/concert/lib/x64_windows_msvc14/stat_mdd/concert.lib>"
        "$<$<CONFIG:Debug>:${CPLEX_STUDIO_DIR}/cplex/lib/x64_windows_msvc14/stat_mdd/ilocplex.lib>"
        "$<$<CONFIG:Debug>:${CPLEX_STUDIO_DIR}/cplex/lib/x64_windows_msvc14/stat_mdd/cplex2010.lib>"

        "$<$<CONFIG:Release>:${CPLEX_STUDIO_DIR}/cpoptimizer/lib/x64_windows_msvc14/stat_mda/cp.lib>"
        "$<$<CONFIG:Release>:${CPLEX_STUDIO_DIR}/concert/lib/x64_windows_msvc14/stat_mda/concert.lib>"
        "$<$<CONFIG:Release>:${CPLEX_STUDIO_DIR}/cplex/lib/x64_windows_msvc14/stat_mda/ilocplex.lib>"
        "$<$<CONFIG:Release>:${CPLEX_STUDIO_DIR}/cplex/lib/x64_windows_msvc14/stat_mda/cplex2010.lib>"
    )

else()

    # Standard IBM ILOG CPLEX Studio Linux layout: <component>/lib/
    # <platform-tag>/<static_pic|static_pic_hybrid>/lib<name>.a, where the
    # platform tag is typically "x86-64_linux" for a 64-bit Linux install.
    # VERIFY this against the ACTUAL installed CPLEX Studio version on the
    # cluster (ls "$CPLEX_STUDIO_DIR/cplex/lib" to see the real directory
    # name) and override CPLEX_LINUX_PLATFORM below if it differs -- no
    # cluster-specific path is guessed here (section 22 of the experiment
    # spec). CPLEX Studio 20.1 (matching this project's Windows build) uses
    # exactly this layout on x86-64 Linux.
    set(CPLEX_LINUX_PLATFORM "x86-64_linux" CACHE STRING
        "Platform subdirectory name under */lib/ in the Linux CPLEX Studio install")

    set(_cplex_lib_dir "${CPLEX_STUDIO_DIR}/cplex/lib/${CPLEX_LINUX_PLATFORM}/static_pic")
    set(_concert_lib_dir "${CPLEX_STUDIO_DIR}/concert/lib/${CPLEX_LINUX_PLATFORM}/static_pic")
    set(_cp_lib_dir "${CPLEX_STUDIO_DIR}/cpoptimizer/lib/${CPLEX_LINUX_PLATFORM}/static_pic")

    # Order matters for GNU ld with static .a archives (single left-to-right
    # pass): a library needing symbols from a later one must come first.
    # -lcp (CP Optimizer) needs Concert; -lilocplex (the IloCplex/IloCplexI
    # C++ wrapper -- distinct from the plain-C libcplex.a) needs both
    # Concert and the base CPLEX library.
    target_link_libraries(drcrffsp PRIVATE
        "${_cp_lib_dir}/libcp.a"
        "${_cplex_lib_dir}/libilocplex.a"
        "${_concert_lib_dir}/libconcert.a"
        "${_cplex_lib_dir}/libcplex.a"
        pthread
        dl
        m
    )

endif()

