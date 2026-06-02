# Test harness. Console, CRT-free, custom entry. Held to a lighter bar than the
# library (/W3, no /WX, no analysis) — matching the old Tests.vcxproj overrides.
add_executable(Tests test_harness.c theme_reduce.hlsl)

# Compile the T6 reduction compute shader to an embedded bytecode header at build time (VS generator's
# FXC integration -> theme_reduce_cs.h, exposing `const BYTE g_themeReduceCS[]`). No runtime D3DCompile
# dependency, so it stays inside the /NODEFAULTLIB build. The shader is data, not linked code.
set_source_files_properties(theme_reduce.hlsl PROPERTIES
  VS_SHADER_TYPE Compute
  VS_SHADER_MODEL 5.0
  VS_SHADER_ENTRYPOINT main
  VS_SHADER_VARIABLE_NAME g_themeReduceCS
  VS_SHADER_OUTPUT_HEADER_FILE "${CMAKE_CURRENT_BINARY_DIR}/theme_reduce_cs.h")

target_include_directories(Tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

target_link_libraries(Tests PRIVATE
  Win32X::Win32X
  kernel32 user32 advapi32 ole32 shell32 shlwapi)

set_target_properties(Tests PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

if(MSVC)
  target_compile_options(Tests PRIVATE /W3 $<$<CONFIG:Release>:/Ot;/Oi>)
  target_link_options(Tests PRIVATE
    /NODEFAULTLIB              # IgnoreAllDefaultLibraries
    /ENTRY:TestEntry
    /SUBSYSTEM:CONSOLE
    /DEBUG)
endif()

if(CMAKE_GENERATOR MATCHES "Visual Studio")
  set_target_properties(Tests PROPERTIES VS_GLOBAL_RunCodeAnalysis false)
endif()

add_test(NAME Win32X.Tests COMMAND Tests)
