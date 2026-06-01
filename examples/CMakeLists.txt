# WinMainEx — GUI demo. WIN32 => /SUBSYSTEM:WINDOWS; app.manifest is embedded
# by the MSVC toolchain (PerMonitorV2 DPI awareness + common controls v6).
add_executable(WinMainEx WIN32 winmain.c app.manifest)

target_link_libraries(WinMainEx PRIVATE
  Win32X::Win32X
  Win32X::warnings
  kernel32 user32 gdi32 winspool comdlg32 advapi32
  shell32 ole32 oleaut32 uuid odbc32 odbccp32)

target_compile_definitions(WinMainEx PRIVATE WIN32 _WINDOWS)

set_target_properties(WinMainEx PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

if(MSVC)
  target_compile_options(WinMainEx PRIVATE
    /permissive-
    # Release: giga-max speed — aggressive inlining (/Ob3), intrinsics, favor
    # speed, atop /O2 + LTCG/LTO.
    $<$<CONFIG:Release>:/Gy;/Ot;/Oi;/Ob3>)
  # CRT-free GUI exe. The static lib supplies __delayLoadHelper2 (delayhlpx.c),
  # so Delayimp.lib (whose delayhlp.obj needs CRT _load_config_used) must NOT be
  # linked. CMake's VS generator folds the slash form /DELAYLOAD into
  # <DelayLoadDLLs>, which auto-appends delayimp.lib. We use the dash form
  # -DELAYLOAD: instead: link.exe honors it, but CMake's flag table doesn't
  # match it, so it lands in raw AdditionalOptions (as the hand-authored project
  # did) and no delayimp.lib is ever added.
  target_link_options(WinMainEx PRIVATE
    /NODEFAULTLIB
    /ENTRY:wWinMainCRTStartup
    /DEBUG
    /DELAYLOAD:user32.dll
    /DELAYLOAD:ole32.dll
    /DELAYLOAD:advapi32.dll
    /DELAYLOAD:shell32.dll)
  # CMake forces /DELAYLOAD into <DelayLoadDLLs>, which appends delayimp.lib.
  # Both that lib and our delayhlpx.obj export the helper; force-reference OURS
  # so it's pulled first and delayimp's CRT-dependent object is never needed.
  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_link_options(WinMainEx PRIVATE /INCLUDE:___delayLoadHelper2@8)
  else()
    target_link_options(WinMainEx PRIVATE /INCLUDE:__delayLoadHelper2)
  endif()
endif()

win32x_enable_analysis(WinMainEx)

# Dev convenience: free the running exe + unregister the shell handler pre-link.
add_custom_command(TARGET WinMainEx PRE_BUILD
  COMMAND powershell -NoProfile -ExecutionPolicy Bypass
          -File "${CMAKE_CURRENT_SOURCE_DIR}/prebuild.ps1"
  COMMENT "WinMainEx dev: kill running instance + unregister handler (free exe for link)"
  VERBATIM)
