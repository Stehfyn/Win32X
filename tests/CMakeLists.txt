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
  kernel32 user32 advapi32 ole32 oleaut32 shell32 shlwapi)

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

# Cross-process message-WAL hook (msgwal.h / msgwal_hook.c): Tests.exe installs WH_GETMESSAGE +
# WH_CALLWNDPROC from this DLL onto the launched WindowsProject.exe GUI thread to log the exact message
# sequence during a theme transition. Ordinary DLL (default CRT) -- it is injected into the app's
# process, which is unrelated to Tests.exe's CRT-free link. Co-located with Tests.exe via the global
# RUNTIME_OUTPUT_DIRECTORY so Tests.exe can LoadLibrary it from its own folder.
add_library(MsgWalHook SHARED msgwal_hook.c)
target_link_libraries(MsgWalHook PRIVATE kernel32 user32)
if(MSVC)
  target_compile_options(MsgWalHook PRIVATE /W3)
endif()
add_dependencies(Tests MsgWalHook)

add_test(NAME Win32X.Tests COMMAND Tests)
