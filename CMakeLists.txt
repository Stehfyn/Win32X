cmake_minimum_required(VERSION 3.21)

project(Win32X
  VERSION 0.1.0
  DESCRIPTION "CRT-free Win32 API extensions"
  LANGUAGES C)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(WIN32X_BUILD_EXAMPLES  "Build the WindowsProject example"           ON)
option(WIN32X_BUILD_TESTS     "Build the test harness"                     ON)
option(WIN32X_ENABLE_ANALYSIS "MSVC /analyze + C++ Core Check (VS only)"   ON)

# ---------------------------------------------------------------------------
# Tree-wide decisions (faithful to the former Directory.Build.props).
# Expressed once here; per-target specifics live in each subdirectory.
# ---------------------------------------------------------------------------
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# CharacterSet=Unicode, tree-wide. The *A translation units (#undef UNICODE
# at their top) opt themselves back out where they need the ANSI entry points.
add_compile_definitions(UNICODE _UNICODE)

# BufferSecurityCheck=false, tree-wide. These targets are CRT-free (executables
# link /NODEFAULTLIB), so the stack-cookie machinery has no runtime to call into.
if(MSVC)
  add_compile_options(/GS-)
  # CMake bakes /Ob2 into the default Release flags; the per-target /Ob3 would
  # then be a second inline-expansion level (cl warning D9025). Strip the
  # default so /Ob3 is the sole /Ob flag in optimized builds.
  string(REPLACE "/Ob2" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
endif()

# Out-of-source artifacts live under the build tree (separate per preset/arch),
# never back in the source tree. Multi-config generators append the config name.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Win32XPolicy.cmake)

add_subdirectory(src)

if(WIN32X_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(WIN32X_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
