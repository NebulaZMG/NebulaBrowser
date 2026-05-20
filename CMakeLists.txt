cmake_minimum_required(VERSION 3.21)

# Enable OBJCXX early on macOS for Cocoa integration
if(APPLE)
    project(NebulaBrowser LANGUAGES CXX OBJCXX)
else()
    project(NebulaBrowser LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------
# CEF location
# ------------------------------------------------------------

set(CEF_ROOT "${CMAKE_SOURCE_DIR}/thirdparty/cef" CACHE PATH "Path to the CEF binary distribution")

if(NOT EXISTS "${CEF_ROOT}/cmake/FindCEF.cmake")
    message(FATAL_ERROR
        "CEF was not found.\n"
        "Expected CEF here:\n"
        "  ${CEF_ROOT}\n\n"
        "Unpack the CEF binary distribution for your OS into thirdparty/cef."
    )
endif()

list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)

add_subdirectory(
    "${CEF_LIBCEF_DLL_WRAPPER_PATH}"
    "${CMAKE_BINARY_DIR}/libcef_dll_wrapper"
)

SET_CEF_TARGET_OUT_DIR()

# ------------------------------------------------------------
# Sources
# ------------------------------------------------------------

set(NEBULA_COMMON_SOURCES
    src/app/first_run_state.cpp
    src/app/nebula_controller.cpp
    src/app/run.cpp
    src/browser/session_state.cpp
    src/browser/tab.cpp
    src/browser/tab_manager.cpp
    src/browser/url_utils.cpp
    src/cef/browser_client.cpp
    src/cef/nebula_app.cpp
    src/ui/paths.cpp
)

if(OS_WINDOWS)
    set(NEBULA_PLATFORM_SOURCES
        src/platform/win/default_browser_win.cpp
        src/platform/win/paths_win.cpp
        src/platform/win/startup_win.cpp
        src/platform/win/browser_host_win.cpp
        src/platform/win/nebula_window_win.cpp
    )
elseif(OS_MACOSX)
    set(NEBULA_PLATFORM_SOURCES
        src/platform/mac/default_browser_mac.mm
        src/platform/mac/paths_mac.cpp
        src/platform/mac/startup_mac.mm
        src/platform/mac/browser_host_mac.mm
        src/platform/mac/nebula_window_mac.mm
    )
    set_source_files_properties(
        src/platform/mac/startup_mac.mm
        src/platform/mac/browser_host_mac.mm
        src/platform/mac/nebula_window_mac.mm
        PROPERTIES
            COMPILE_FLAGS "-fobjc-arc"
    )
elseif(OS_LINUX)
    set(NEBULA_PLATFORM_SOURCES
        src/platform/linux/default_browser_linux.cpp
        src/platform/linux/paths_linux.cpp
        src/platform/linux/startup_linux.cpp
        src/platform/linux/browser_host_linux.cpp
        src/platform/linux/nebula_window_linux.cpp
    )
else()
    message(FATAL_ERROR "Unsupported platform.")
endif()

# On macOS, CEF is a framework linked via CEF_STANDARD_LIBS.
# On Windows/Linux, we create a logical target for libcef.
if(NOT OS_MACOSX)
    ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_RELEASE}" "${CEF_LIB_DEBUG}")
endif()

if(OS_LINUX)
    FIND_LINUX_LIBRARIES("X11")
endif()

function(add_nebula_app_target nebula_target entry_source)
    if(OS_WINDOWS)
        add_executable(${nebula_target} WIN32
            ${entry_source}
            ${NEBULA_COMMON_SOURCES}
            ${NEBULA_PLATFORM_SOURCES}
        )
    elseif(OS_MACOSX)
        add_executable(${nebula_target} MACOSX_BUNDLE
            ${entry_source}
            ${NEBULA_COMMON_SOURCES}
            ${NEBULA_PLATFORM_SOURCES}
        )
    elseif(OS_LINUX)
        add_executable(${nebula_target}
            ${entry_source}
            ${NEBULA_COMMON_SOURCES}
            ${NEBULA_PLATFORM_SOURCES}
        )
    endif()

    SET_EXECUTABLE_TARGET_PROPERTIES(${nebula_target})
    add_dependencies(${nebula_target} libcef_dll_wrapper)

    target_include_directories(${nebula_target} PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${CEF_ROOT}"
        "${CEF_ROOT}/include"
    )

    if(OS_MACOSX)
        # On macOS, CEF is a framework; don't link libcef_lib
        target_link_libraries(${nebula_target} PRIVATE
            libcef_dll_wrapper
            ${CEF_STANDARD_LIBS}
        )
    else()
        target_link_libraries(${nebula_target} PRIVATE
            libcef_lib
            libcef_dll_wrapper
            ${CEF_STANDARD_LIBS}
        )
    endif()

    if(MSVC)
        set_property(TARGET ${nebula_target} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
    endif()

    # ------------------------------------------------------------
    # Platform-specific CEF runtime deployment
    # ------------------------------------------------------------

    if(OS_WINDOWS)
        target_link_libraries(${nebula_target} PRIVATE dwmapi)
        target_compile_definitions(${nebula_target} PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
        )
        COPY_FILES("${nebula_target}" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR_RELEASE}" "${CEF_TARGET_OUT_DIR}")
        COPY_FILES("${nebula_target}" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${CEF_TARGET_OUT_DIR}")
    elseif(OS_LINUX)
        COPY_FILES("${nebula_target}" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR_RELEASE}" "${CEF_TARGET_OUT_DIR}")
        COPY_FILES("${nebula_target}" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${CEF_TARGET_OUT_DIR}")
    elseif(OS_MACOSX)
        target_link_libraries(${nebula_target} PRIVATE
            "-framework Cocoa"
            "-framework ApplicationServices"
        )
        set(NEBULA_APP "${CEF_TARGET_OUT_DIR}/${nebula_target}.app")
        set(NEBULA_HELPER_TARGET "${nebula_target}_Helper")
        set(NEBULA_HELPER_OUTPUT_NAME "${nebula_target} Helper")
        string(TOLOWER "${nebula_target}" NEBULA_HELPER_BUNDLE_NAME)

        COPY_MAC_FRAMEWORK(
            "${nebula_target}"
            "${CEF_BINARY_DIR_RELEASE}"
            "${NEBULA_APP}"
        )
        COPY_FILES(
            "${nebula_target}"
            "${CEF_BINARY_FILES}"
            "${CEF_BINARY_DIR_RELEASE}"
            "${NEBULA_APP}/Contents/Frameworks"
        )
        COPY_FILES(
            "${nebula_target}"
            "${CEF_RESOURCE_FILES}"
            "${CEF_RESOURCE_DIR}"
            "${NEBULA_APP}/Contents/Resources"
        )

        foreach(_suffix_list ${CEF_HELPER_APP_SUFFIXES})
            string(REPLACE ":" ";" _suffix_list ${_suffix_list})
            list(GET _suffix_list 0 _name_suffix)
            list(GET _suffix_list 1 _target_suffix)
            list(GET _suffix_list 2 _plist_suffix)

            set(_helper_target "${NEBULA_HELPER_TARGET}${_target_suffix}")
            set(_helper_output_name "${NEBULA_HELPER_OUTPUT_NAME}${_name_suffix}")
            set(_helper_info_plist "${CMAKE_CURRENT_BINARY_DIR}/${_helper_target}-Info.plist")

            file(READ "${CMAKE_SOURCE_DIR}/cmake/mac-helper-Info.plist.in" _plist_contents)
            string(REPLACE "\${EXECUTABLE_NAME}" "${_helper_output_name}" _plist_contents "${_plist_contents}")
            string(REPLACE "\${PRODUCT_NAME}" "${_helper_output_name}" _plist_contents "${_plist_contents}")
            string(REPLACE "\${HELPER_BUNDLE_NAME}" "${NEBULA_HELPER_BUNDLE_NAME}" _plist_contents "${_plist_contents}")
            string(REPLACE "\${BUNDLE_ID_SUFFIX}" "${_plist_suffix}" _plist_contents "${_plist_contents}")
            file(WRITE "${_helper_info_plist}" "${_plist_contents}")

            add_executable(${_helper_target} MACOSX_BUNDLE
                app/process_helper_mac.cc
                src/cef/nebula_app.cpp
            )
            SET_EXECUTABLE_TARGET_PROPERTIES(${_helper_target})
            add_dependencies(${_helper_target} libcef_dll_wrapper)
            target_include_directories(${_helper_target} PRIVATE
                "${CMAKE_SOURCE_DIR}/src"
                "${CEF_ROOT}"
                "${CEF_ROOT}/include"
            )
            target_link_libraries(${_helper_target} PRIVATE
                libcef_dll_wrapper
                ${CEF_STANDARD_LIBS}
            )
            set_target_properties(${_helper_target} PROPERTIES
                MACOSX_BUNDLE_INFO_PLIST "${_helper_info_plist}"
                OUTPUT_NAME "${_helper_output_name}"
            )

            add_dependencies(${nebula_target} "${_helper_target}")
            add_custom_command(TARGET ${nebula_target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CEF_TARGET_OUT_DIR}/${_helper_output_name}.app"
                    "${NEBULA_APP}/Contents/Frameworks/${_helper_output_name}.app"
                VERBATIM
            )
        endforeach()
    endif()

    # ------------------------------------------------------------
    # Copy Nebula UI files after build
    # ------------------------------------------------------------

    add_custom_command(TARGET ${nebula_target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/ui"
            "$<TARGET_FILE_DIR:${nebula_target}>/ui"

        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:${nebula_target}>/ui/assets"

        COMMENT "Copying Nebula UI files and assets for ${nebula_target}..."
    )
endfunction()

add_nebula_app_target(NebulaBrowser app/main.cpp)
add_nebula_app_target(NebulaBigPicture app/main_bigpicture.cpp)
