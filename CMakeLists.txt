# crackdown - ReXGlue Recompiled Project
#
# SDK wiring for generated/default lives in cmake/rexglue_pregenerated.cmake.

cmake_minimum_required(VERSION 3.25)
project(crackdown VERSION 0.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Where tools/codegen.ps1 and tools/package-linux.sh unpack the pinned SDK.
if(WIN32)
    set(CRACKDOWN_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/rexglue/win-amd64")
else()
    set(CRACKDOWN_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/rexglue/linux-amd64")
endif()
list(APPEND CMAKE_PREFIX_PATH "${CRACKDOWN_SDK_DIR}")

get_property(_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _multi_config AND NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo|Release)$")
    message(FATAL_ERROR "Set CMAKE_BUILD_TYPE to Debug, RelWithDebInfo or Release (the presets and tools/build.ps1 "
        "do): the ReXGlue runtime and GPU plugin are checked per configuration")
endif()

# Release binaries must not carry the build machine's folders (__FILE__ in log calls).
add_compile_options("$<$<CONFIG:Release>:-fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.>")

include(cmake/rexglue_pregenerated.cmake)

# TRG Launcher: the third_party/trg-launcher submodule. A local checkout named by
# CRACKDOWN_TRG_LAUNCHER_DIR is used when the submodule is not checked out.
set(CRACKDOWN_TRG_LAUNCHER_DIR "$ENV{CRACKDOWN_TRG_LAUNCHER_DIR}" CACHE PATH
    "Local TRG Launcher checkout, used when third_party/trg-launcher is empty")

# Upstream TRG is immutable. Every build compiles a patched copy under the binary directory; the
# submodule checkout must stay at the pinned revision.
set(CRACKDOWN_TRG_LAUNCHER_REVISION "b384a28c712c9320eea8f9d50639dec49981b6c1")
set(CRACKDOWN_TRG_LAUNCHER_PATCH_DIR "${CMAKE_CURRENT_SOURCE_DIR}/patches/trg-launcher")
set(CRACKDOWN_TRG_LAUNCHER_PATCH_STAMP_VERSION 1)
file(GLOB _trg_patch_names CONFIGURE_DEPENDS RELATIVE "${CRACKDOWN_TRG_LAUNCHER_PATCH_DIR}"
    "${CRACKDOWN_TRG_LAUNCHER_PATCH_DIR}/*.patch")
list(SORT _trg_patch_names)
# The glob dependency only tracks added or removed files; list the patches as configure dependencies
# too so editing one triggers a reconfigure.
foreach(_trg_patch_name IN LISTS _trg_patch_names)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CRACKDOWN_TRG_LAUNCHER_PATCH_DIR}/${_trg_patch_name}")
endforeach()

find_package(Git QUIET)
if(NOT Git_FOUND)
    message(FATAL_ERROR "TRG Launcher: git is required to apply patches/trg-launcher")
endif()

# Copies the source into output_dir and applies the patches there. The stamp covers the source state
# and every patch, so the copy is rebuilt only when one of them changes. GIT_CEILING_DIRECTORIES
# keeps git apply from discovering the parent repository when the generated copy has no .git of its
# own.
function(crackdown_prepare_patched_trg_launcher source_dir output_dir require_pinned)
    set(_source_state "")
    if(EXISTS "${source_dir}/.git")
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source_dir}" rev-parse HEAD
                        OUTPUT_VARIABLE _head OUTPUT_STRIP_TRAILING_WHITESPACE
                        RESULT_VARIABLE _head_result ERROR_QUIET)
        if(NOT _head_result EQUAL 0)
            message(FATAL_ERROR "TRG Launcher: ${source_dir} is not a git checkout; run `git submodule update --init third_party/trg-launcher`")
        endif()
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source_dir}" status --porcelain --untracked-files=normal
                        OUTPUT_VARIABLE _status OUTPUT_STRIP_TRAILING_WHITESPACE)
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source_dir}" diff --binary HEAD
                        OUTPUT_VARIABLE _diff)
        string(SHA256 _source_state "${_head}\n${_status}\n${_diff}")
        if(require_pinned)
            if(NOT _head STREQUAL CRACKDOWN_TRG_LAUNCHER_REVISION)
                message(FATAL_ERROR "TRG Launcher: ${source_dir} is at ${_head}, expected the pinned ${CRACKDOWN_TRG_LAUNCHER_REVISION}. Run `git submodule update --init third_party/trg-launcher`.")
            endif()
            if(NOT _status STREQUAL "")
                message(FATAL_ERROR "TRG Launcher: ${source_dir} has local changes; run `git -C third_party/trg-launcher checkout .` and configure again.")
            endif()
        endif()
    else()
        file(GLOB_RECURSE _source_files LIST_DIRECTORIES false RELATIVE "${source_dir}" "${source_dir}/*")
        list(SORT _source_files)
        foreach(_file IN LISTS _source_files)
            if(_file MATCHES "(^|/)\\.git(/|$)")
                continue()
            endif()
            file(SHA256 "${source_dir}/${_file}" _file_hash)
            string(APPEND _source_state "${_file}\n${_file_hash}\n")
        endforeach()
        string(SHA256 _source_state "${_source_state}")
    endif()

    set(_fingerprint "${CRACKDOWN_TRG_LAUNCHER_PATCH_STAMP_VERSION}\n${source_dir}\n${_source_state}\n")
    foreach(_patch_name IN LISTS _trg_patch_names)
        file(SHA256 "${CRACKDOWN_TRG_LAUNCHER_PATCH_DIR}/${_patch_name}" _patch_hash)
        string(APPEND _fingerprint "${_patch_name}\n${_patch_hash}\n")
    endforeach()
    string(SHA256 _fingerprint "${_fingerprint}")

    set(_stamp "${output_dir}/.crackdown-trg-patch-stamp")
    if(EXISTS "${_stamp}" AND EXISTS "${output_dir}/CMakeLists.txt")
        file(READ "${_stamp}" _stamp_value)
        if(_stamp_value STREQUAL _fingerprint)
            return()
        endif()
    endif()

    message(STATUS "TRG Launcher: preparing patched copy in ${output_dir}")
    file(REMOVE_RECURSE "${output_dir}")
    file(MAKE_DIRECTORY "${output_dir}")
    file(COPY "${source_dir}/" DESTINATION "${output_dir}" PATTERN ".git" EXCLUDE)

    get_filename_component(_ceiling "${output_dir}" DIRECTORY)
    foreach(_patch_name IN LISTS _trg_patch_names)
        set(_patch "${CRACKDOWN_TRG_LAUNCHER_PATCH_DIR}/${_patch_name}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E env "GIT_CEILING_DIRECTORIES=${_ceiling}"
                        "${GIT_EXECUTABLE}" -C "${output_dir}" apply --ignore-whitespace --check "${_patch}"
                        RESULT_VARIABLE _can_apply ERROR_VARIABLE _apply_error)
        if(NOT _can_apply EQUAL 0)
            message(FATAL_ERROR "TRG Launcher: ${_patch} does not apply to ${source_dir} (${_apply_error}); the patch and the source revision are out of sync.")
        endif()
        execute_process(COMMAND "${CMAKE_COMMAND}" -E env "GIT_CEILING_DIRECTORIES=${_ceiling}"
                        "${GIT_EXECUTABLE}" -C "${output_dir}" apply --ignore-whitespace "${_patch}"
                        RESULT_VARIABLE _apply_result)
        if(NOT _apply_result EQUAL 0)
            message(FATAL_ERROR "TRG Launcher: failed to apply ${_patch}")
        endif()
    endforeach()
    file(WRITE "${_stamp}" "${_fingerprint}")
endfunction()

set(_trg_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/third_party/trg-launcher")
if(EXISTS "${_trg_source_dir}/CMakeLists.txt")
    set(_trg_require_pinned TRUE)
else()
    set(_trg_source_dir "${CRACKDOWN_TRG_LAUNCHER_DIR}")
    set(_trg_require_pinned FALSE)
endif()
if(NOT EXISTS "${_trg_source_dir}/CMakeLists.txt")
    message(FATAL_ERROR "TRG Launcher not found: run `git submodule update --init third_party/trg-launcher`")
endif()

set(_trg_patched_dir "${CMAKE_BINARY_DIR}/trg-launcher-patched")
crackdown_prepare_patched_trg_launcher("${_trg_source_dir}" "${_trg_patched_dir}" ${_trg_require_pinned})

# Reconfigure when the patches, the upstream checkout or its HEAD change.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${_trg_source_dir}/CMakeLists.txt")
if(EXISTS "${_trg_source_dir}/.git")
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${_trg_source_dir}" rev-parse --git-path HEAD
                    OUTPUT_VARIABLE _trg_head_path OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_trg_head_path)
        get_filename_component(_trg_head_path "${_trg_head_path}" ABSOLUTE BASE_DIR "${_trg_source_dir}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_trg_head_path}")
    endif()
endif()

# The launcher draws inside the runtime's own Dear ImGui frame, so it must use the ImGui compiled into rexruntime
# (one context), never a downloaded copy.
add_library(crackdown_imgui INTERFACE)
target_link_libraries(crackdown_imgui INTERFACE rex::runtime)
set(TRG_LAUNCHER_IMGUI_TARGET crackdown_imgui CACHE STRING "" FORCE)
set(TRG_LAUNCHER_STANDALONE OFF CACHE BOOL "" FORCE)
set(TRG_LAUNCHER_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(TRG_LAUNCHER_FETCH_DEPS OFF CACHE BOOL "" FORCE)
add_subdirectory("${_trg_patched_dir}" trg-launcher)
# Written for C++17: in C++20 and later u8string() is a different type.
set_target_properties(trg_launcher PROPERTIES CXX_STANDARD 17)
# TRG defines this only for cl.exe; clang++ needs it too for the MSVC CRT.
target_compile_definitions(trg_launcher PRIVATE _CRT_SECURE_NO_WARNINGS)
get_directory_property(CRACKDOWN_TRG_VERSION DIRECTORY "${_trg_patched_dir}" DEFINITION PROJECT_VERSION)

set(CRACKDOWN_SOURCES
    src/main.cpp
    src/achievements_input.cpp
    src/achievements_overlay.cpp
    src/active_log.cpp
    src/aspect_render.cpp
    src/audio_mixer_priority.cpp
    src/audio_guest_sync.cpp
    src/audio_pcm_firewall.cpp
    src/audio_wave_timeout.cpp
    src/cache_device.cpp
    src/character_step_fixes.cpp
    src/cheat_hooks.cpp
    src/cheat_menu_input.cpp
    src/cheat_menu_overlay.cpp
    src/cheats.cpp
    src/crash_report.cpp
    src/crt_native.cpp
    src/entity_registry.cpp
    src/font_atlas.cpp
    src/font_rerender.cpp
    src/frame_counter.cpp
    src/frame_pacing.cpp
    src/frame_rate.cpp
    src/game_files.cpp
    src/glyph_overlay.cpp
    src/gpu_fence_wait.cpp
    src/graphics_adapters.cpp
    src/guest_fault_log.cpp
    src/hang_watchdog.cpp
    src/launcher.cpp
    src/launcher_achievements.cpp
    src/launcher_achievements_page.cpp
    src/log_filter.cpp
    src/log_path.cpp
    src/main_menu_quit.cpp
    src/massive_server_guard.cpp
    src/mouse_look.cpp
    src/nan_guards.cpp
    src/orb_map.cpp
    src/platform.cpp
    src/session_log.cpp
    src/settings.cpp
    src/settings_file.cpp
    src/title_compat.cpp
    src/title_language.cpp
    src/update_config.cpp
    src/updater/github_release.cpp
    src/updater/json.cpp
    src/updater/sha256.cpp
    src/updater/third_party/miniz/miniz.c
    src/updater/update_controller.cpp
    src/updater/update_job.cpp
    src/updater/update_paths.cpp
    src/updater/update_platform.cpp
    src/updater/update_ui.cpp
    src/updater/update_zip.cpp
    src/updater/version.cpp
    src/user_data.cpp
    src/visibility_grid_guard.cpp
    src/window_host.cpp
    src/xdvdfs.cpp
    src/skip_intro.cpp
)
set_source_files_properties(src/updater/third_party/miniz/miniz.c PROPERTIES COMPILE_OPTIONS "-w")
# Stricter checks for the project's own C++ only: not the generated code, the SDK headers (system includes) or the SDK
# sources rexglue_setup_target adds to this target.
set(CRACKDOWN_WARNINGS -Wall -Wextra
    -Wshadow-all -Wimplicit-fallthrough -Wnull-dereference -Wformat=2 -Wundef -Wcast-qual -Wextra-semi
    -Wunreachable-code-aggressive -Wconditional-uninitialized -Wloop-analysis -Wrange-loop-analysis -Wcomma
    -Wunused-member-function -Wunused-template -Wdeprecated -Wsuggest-override -Wnewline-eof)
set_property(SOURCE ${CRACKDOWN_SOURCES} APPEND PROPERTY COMPILE_OPTIONS
    "$<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:${CRACKDOWN_WARNINGS}>")

if(WIN32)
    # Resource 1 is the first icon in the executable: Explorer, and SDL's window class, use it. The icon is
    # generated from packaging/icons/app-icon.svg by tools/make-app-icon.py and is committed to the repository.
    enable_language(RC)
    set(CRACKDOWN_ICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icons/app-icon.ico")
    if(EXISTS "${CRACKDOWN_ICON}")
        set(CRACKDOWN_RC_ICON_LINE "1 ICON \"${CRACKDOWN_ICON}\"")
    else()
        set(CRACKDOWN_RC_ICON_LINE "")
        message(STATUS "No packaging/icons/app-icon.ico: the executable gets no icon (run tools/make-app-icon.py)")
    endif()
    string(REPLACE "\\" "/" CRACKDOWN_RC_ICON_LINE "${CRACKDOWN_RC_ICON_LINE}")
    set(CRACKDOWN_VERSION_COMMA "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
    configure_file(packaging/windows/crackdown.rc.in "${CMAKE_CURRENT_BINARY_DIR}/crackdown.rc" @ONLY)
    if(EXISTS "${CRACKDOWN_ICON}")
        set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/crackdown.rc" PROPERTIES OBJECT_DEPENDS "${CRACKDOWN_ICON}")
    endif()
    list(APPEND CRACKDOWN_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/crackdown.rc")
    add_executable(crackdown WIN32 ${CRACKDOWN_SOURCES})
    set(CRACKDOWN_APP_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/crackdown.manifest")
    target_link_options(crackdown PRIVATE "LINKER:/MANIFESTINPUT:${CRACKDOWN_APP_MANIFEST}")
    set_property(TARGET crackdown APPEND PROPERTY LINK_DEPENDS "${CRACKDOWN_APP_MANIFEST}")
else()
    add_executable(crackdown ${CRACKDOWN_SOURCES})
endif()

rexglue_setup_target(crackdown GPU_PLUGINS xenos)
target_link_libraries(crackdown PRIVATE trg::launcher)
target_compile_definitions(crackdown PRIVATE
    CRACKDOWN_PORT_VERSION="${PROJECT_VERSION}"
    CRACKDOWN_TRG_VERSION="${CRACKDOWN_TRG_VERSION}"
    # CD_UPDATE_TEST* environment overrides for manual updater testing; release packages are built as Release.
    $<$<NOT:$<CONFIG:Release>>:CRACKDOWN_UPDATE_TEST_OVERRIDES>)

if(WIN32)
    target_link_libraries(crackdown PRIVATE shcore synchronization dxgi dbghelp)
    # The SDK stages its DLLs only when the program links, so a rebuilt runtime or GPU plugin has to make it link
    # again; the check and staging targets below and tools/build.ps1 also verify the staged copies.
    foreach(_sdk_dll rexruntime rexruntimed rexruntimerd rexgpu-xenos rexgpu-xenosd rexgpu-xenosrd)
        if(EXISTS "${CRACKDOWN_SDK_DIR}/bin/${_sdk_dll}.dll")
            set_property(TARGET crackdown APPEND PROPERTY LINK_DEPENDS "${CRACKDOWN_SDK_DIR}/bin/${_sdk_dll}.dll")
        endif()
    endforeach()
else()
    # The SDK stages the runtime beside the program only on Windows and macOS; the RPATH is $ORIGIN.
    add_custom_command(TARGET crackdown POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:rex::runtime>" "$<TARGET_FILE_DIR:crackdown>"
        VERBATIM
    )
    foreach(_sdk_lib rexruntime rexruntimed rexruntimerd rexgpu-xenos rexgpu-xenosd rexgpu-xenosrd)
        if(EXISTS "${CRACKDOWN_SDK_DIR}/lib/lib${_sdk_lib}.so")
            set_property(TARGET crackdown APPEND PROPERTY LINK_DEPENDS "${CRACKDOWN_SDK_DIR}/lib/lib${_sdk_lib}.so")
        endif()
    endforeach()
endif()

# Every build checks that tools/rexglue holds the runtime and GPU plugin built from the patches, and that the
# copies next to the program are those; tools/build.ps1 and tools/build.sh rebuild them when they are not.
if(WIN32)
    # Windows PowerShell has a fixed path, unlike a Store pwsh; the module path a parent pwsh sets breaks it.
    find_program(CRACKDOWN_WINDOWS_POWERSHELL NAMES powershell REQUIRED)
    set(CRACKDOWN_REXGLUE_CHECK "${CMAKE_COMMAND}" -E env --unset=PSModulePath
        "${CRACKDOWN_WINDOWS_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/build-patched-rexruntime.ps1" -Check -Config $<CONFIG>)
    set(CRACKDOWN_REXGLUE_STAGED -Staged)
else()
    find_program(CRACKDOWN_BASH bash REQUIRED)
    set(CRACKDOWN_REXGLUE_CHECK "${CRACKDOWN_BASH}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/build-patched-rexruntime.sh"
        --check "${CRACKDOWN_SDK_DIR}" --config $<CONFIG>)
    set(CRACKDOWN_REXGLUE_STAGED --staged)
endif()
add_custom_target(crackdown_rexglue_check COMMAND ${CRACKDOWN_REXGLUE_CHECK} VERBATIM)
add_dependencies(crackdown crackdown_rexglue_check)
add_custom_target(crackdown_rexglue_staging ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:rex::runtime>" "$<TARGET_FILE:rex::gpu-xenos>"
        "$<TARGET_FILE_DIR:crackdown>"
    COMMAND ${CRACKDOWN_REXGLUE_CHECK} ${CRACKDOWN_REXGLUE_STAGED} "$<TARGET_FILE_DIR:crackdown>"
    VERBATIM
)
add_dependencies(crackdown_rexglue_staging crackdown)

add_custom_command(TARGET crackdown POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:crackdown>/assets/launcher"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/launcher/header-art.png"
        "$<TARGET_FILE_DIR:crackdown>/assets/launcher/header-art.png"
    VERBATIM
)

# ThinLTO in Release and RelWithDebInfo; the runtime and GPU plugin are prebuilt. Debug compiles at -O0, which marks
# every function optnone, so LTO would optimize nothing there.
function(crackdown_enable_thinlto target)
    set(_lto "$<CONFIG:Release,RelWithDebInfo>")
    if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
        # lld-link runs the LTO backend at -O2 unless told otherwise; Release compiles at -O3.
        set(_link -flto=thin "$<$<CONFIG:Release>:-Wl,/opt:lldlto=3>"
            "-Wl,/lldltocache:${CMAKE_BINARY_DIR}/lto-cache")
    elseif(UNIX AND NOT APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(_link -flto=thin -fuse-ld=lld "-Wl,--thinlto-cache-dir=${CMAKE_BINARY_DIR}/lto-cache")
    else()
        return()
    endif()
    target_compile_options(${target} PRIVATE "$<$<AND:${_lto},$<COMPILE_LANGUAGE:CXX>>:-flto=thin>")
    get_target_property(_type ${target} TYPE)
    if(NOT _type STREQUAL "OBJECT_LIBRARY")
        target_link_options(${target} PRIVATE "$<${_lto}:${_link}>")
    endif()
endfunction()
crackdown_enable_thinlto(crackdown)
if(TARGET crackdown_recomp)
    crackdown_enable_thinlto(crackdown_recomp)
endif()

# The update helper runs from the temporary update workspace, away from the installation, so it keeps its runtime
# inside the executable.
if(WIN32)
    add_executable(trg-updater WIN32
        src/updater/helper_main.cpp
        src/updater/install_transaction.cpp
        src/updater/sha256.cpp
        src/updater/update_job.cpp
        src/updater/update_paths.cpp
        src/updater/update_platform.cpp
    )
    target_include_directories(trg-updater PRIVATE src)
    target_link_libraries(trg-updater PRIVATE shell32)
    target_link_options(trg-updater PRIVATE -static)
    set_property(TARGET trg-updater PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(trg-updater PRIVATE -Wall -Wextra)
    set_target_properties(trg-updater PROPERTIES OUTPUT_NAME "trg-updater")
    crackdown_enable_thinlto(trg-updater)
endif()

# The runtime reads its cvars from <exe dir>/crackdown.toml.
add_custom_command(TARGET crackdown POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/crackdown.toml"
        "$<TARGET_FILE_DIR:crackdown>"
    VERBATIM
)

# The button glyph sheet the font overlay reads from <exe dir>/glyphs (src/glyph_overlay.cpp).
add_custom_command(TARGET crackdown POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:crackdown>/glyphs"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/glyphs/button_glyphs.dds"
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/glyphs/button_glyphs.toml"
        "$<TARGET_FILE_DIR:crackdown>/glyphs"
    VERBATIM
)

# Runtime-linked tests, not part of the default build. Each copies the runtime next to itself; tools/test.ps1 runs the
# pure-logic tests in tests/ without the runtime.
function(crackdown_add_runtime_test name source)
    add_executable(${name} EXCLUDE_FROM_ALL "tests/${source}")
    target_link_libraries(${name} PRIVATE rex::runtime)
    target_compile_options(${name} PRIVATE -Wall -Wextra)
    add_custom_command(TARGET ${name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:rex::runtime>" "$<TARGET_FILE_DIR:${name}>"
        VERBATIM
    )
    if(NOT WIN32)
        set_target_properties(${name} PROPERTIES BUILD_RPATH "$ORIGIN")
    endif()
endfunction()
crackdown_add_runtime_test(crackdown_physical_heap_stress physical_heap_stress.cpp)
crackdown_add_runtime_test(crackdown_title_thread_shutdown_tests title_thread_shutdown_tests.cpp)
crackdown_add_runtime_test(crackdown_shutdown_order_tests shutdown_order_tests.cpp)

# Log file selection and the Detailed logging filter need the runtime's logger registry.
function(crackdown_add_logging_test name)
    add_executable(${name} EXCLUDE_FROM_ALL ${ARGN})
    target_include_directories(${name} PRIVATE src)
    target_compile_options(${name} PRIVATE -Wall -Wextra)
    target_link_libraries(${name} PRIVATE rex::runtime)
    if(WIN32)
        target_link_libraries(${name} PRIVATE shell32)
    endif()
    add_custom_command(TARGET ${name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:rex::runtime>" "$<TARGET_FILE_DIR:${name}>"
        VERBATIM
    )
    if(NOT WIN32)
        set_target_properties(${name} PROPERTIES BUILD_RPATH "$ORIGIN")
    endif()
endfunction()
crackdown_add_logging_test(crackdown_active_log_tests
    tests/active_log_tests.cpp
    src/active_log.cpp
    src/log_filter.cpp
)
crackdown_add_logging_test(crackdown_log_filter_tests
    tests/log_filter_tests.cpp
    src/active_log.cpp
    src/crash_report.cpp
    src/log_filter.cpp
    src/session_log.cpp
)
target_compile_definitions(crackdown_log_filter_tests PRIVATE CRACKDOWN_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src")

# RenderDoc detection test (Linux; the fake library goes into renderdoc/, out of reach of the runtime's RUNPATH).
if(UNIX AND NOT APPLE)
    add_library(crackdown_fake_renderdoc SHARED EXCLUDE_FROM_ALL tests/fake_renderdoc.cpp)
    set_target_properties(crackdown_fake_renderdoc PROPERTIES OUTPUT_NAME renderdoc
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/renderdoc")
    crackdown_add_runtime_test(crackdown_renderdoc_detect_tests renderdoc_detect_tests.cpp)
    target_link_libraries(crackdown_renderdoc_detect_tests PRIVATE rex::renderdoc ${CMAKE_DL_LIBS})
    add_dependencies(crackdown_renderdoc_detect_tests crackdown_fake_renderdoc)
endif()

# Writes the decrypted guest image of the supported XEX (the input of tools/make-app-icon.py and of the analysis
# tools): cmake --build <dir> --target crackdown_dump_image
add_executable(crackdown_dump_image EXCLUDE_FROM_ALL tools/dump_image/main.cpp)
target_link_libraries(crackdown_dump_image PRIVATE rex::runtime)
add_custom_command(TARGET crackdown_dump_image POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:rex::runtime>" "$<TARGET_FILE_DIR:crackdown_dump_image>"
    VERBATIM
)
