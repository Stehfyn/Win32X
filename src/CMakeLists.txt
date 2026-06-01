# Win32X — the static library. Public API lives in ../include/Win32X; the .c,
# the *.inl thunks, and the internal result.h live here.
add_library(Win32X STATIC
  WinBaseX.c
  WinBaseXStartupA.c
  WinBaseXStartupW.c
  WinUserX.c
  ShellScalingApiX.c
  versionhelpersx.c
  delayhlpx.c
  processenvxA.c
  processenvxW.c)
add_library(Win32X::Win32X ALIAS Win32X)

# PUBLIC include/ => consumers (tests, examples, downstream) get the
# `#include "Win32X/..."` search root for free. PRIVATE here for result.h/*.inl.
target_include_directories(Win32X
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(Win32X PRIVATE WIN32_LEAN_AND_MEAN COBJMACROS)

if(MSVC)
  target_compile_options(Win32X PRIVATE
    /permissive-                 # ConformanceMode=true
    /Gy                          # FunctionLevelLinking=true
    # Release = giga-max speed favoring speed: aggressive inlining (/Ob3),
    # intrinsics (/Oi), favor-speed (/Ot), on top of /O2 + LTCG/LTO (/GL +
    # /LTCG via INTERPROCEDURAL_OPTIMIZATION below).
    $<$<CONFIG:Release>:/Ot;/Oi;/Ob3>)
endif()

# WholeProgramOptimization=true in Release -> /GL on compile, /LTCG at link.
set_target_properties(Win32X PROPERTIES
  INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# BUILD_INTERFACE: the warnings policy is a build-only compile dependency; a
# static lib otherwise promotes PRIVATE links into its export set.
target_link_libraries(Win32X PRIVATE $<BUILD_INTERFACE:win32x_warnings>)
win32x_enable_analysis(Win32X)

# ---------------------------------------------------------------------------
# install / export  -> find_package(Win32X) + Win32X::Win32X downstream.
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS Win32X EXPORT Win32XTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/Win32X
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT Win32XTargets
  NAMESPACE Win32X::
  FILE Win32XTargets.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Win32X)

configure_package_config_file(
  ${CMAKE_SOURCE_DIR}/cmake/Win32XConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/Win32XConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Win32X)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/Win32XConfigVersion.cmake
  COMPATIBILITY SameMajorVersion)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/Win32XConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/Win32XConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Win32X)
