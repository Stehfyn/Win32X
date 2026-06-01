# Test harness. Console, CRT-free, custom entry. Held to a lighter bar than the
# library (/W3, no /WX, no analysis) — matching the old Tests.vcxproj overrides.
add_executable(Tests test_harness.c)

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
