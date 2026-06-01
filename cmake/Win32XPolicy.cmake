# Win32XPolicy.cmake — shared build policy (replaces Directory.Build.props).
# Decisions are stated once here and propagated through usage requirements,
# so a consumer that links Win32X::warnings inherits the whole policy.

# --- Strict warnings -------------------------------------------------------
# Warnings-as-errors with ZERO /wd suppressions. Every *enabled* warning is an
# error, no exceptions. The enabled SET is per-config, because an optimized
# build provably cannot be /Wall-clean:
#   - The optimizer selects functions for inline expansion. If it inlines ->
#     C4711; if it declines -> C4710. The pair is mutually exclusive: one always
#     fires. Neither is a defect or fixable in source (verified: /Ob0 silences
#     C4711 only by trading it for C4710 *and* killing the inlining we want).
#   - C5045 (Spectre) is likewise an off-by-default /Wall informational warning.
# So: Debug = /Wall (optimizer off -> genuinely clean, maximal checking).
#     Release = /W4 (the C47xx/C48xx/C50xx informational warnings are off-by-
#     default here, so aggressive inlining stays clean). Nothing is suppressed.
# C4820 padding is fixed structurally regardless (explicit struct padding), so
# /Wall stays clean in Debug. /external:W0 excludes the Windows SDK headers from
# analysis — external code, not our diagnostics.
add_library(win32x_warnings INTERFACE)
add_library(Win32X::warnings ALIAS win32x_warnings)

if(MSVC)
  target_compile_options(win32x_warnings INTERFACE
    /Wall /WX
    /external:anglebrackets /external:W0)
else()
  # Same decision — every warning an error, nothing disabled — other toolchains.
  target_compile_options(win32x_warnings INTERFACE
    -Wall -Wextra -Werror)
endif()

# --- Static analysis -------------------------------------------------------
# CppCoreCheck + Microsoft code analysis (NativeRecommendedRules). Reproduced
# through the VS-generated project, where the IDE resolves the ruleset by name;
# only meaningful under the Visual Studio generator, and gated by the option.
function(win32x_enable_analysis target)
  if(WIN32X_ENABLE_ANALYSIS AND CMAKE_GENERATOR MATCHES "Visual Studio")
    set_target_properties(${target} PROPERTIES
      VS_GLOBAL_RunCodeAnalysis             true
      VS_GLOBAL_EnableMicrosoftCodeAnalysis true
      VS_GLOBAL_EnableCppCoreCheck          true
      VS_GLOBAL_CodeAnalysisRuleSet         NativeRecommendedRules.ruleset)
  endif()
endfunction()
