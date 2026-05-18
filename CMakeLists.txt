cmake_minimum_required(VERSION 3.21)

project(NebulaBrowser LANGUAGES CXX)

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
        src/platform/win/paths_win.cpp
        src/platform/win/startup_win.cpp
        src/platform/win/browser_host_win.cpp
        src/platform/win/nebula_window_win.cpp
    )
elseif(OS_MACOSX)
    set(NEBULA_PLATFORM_SOURCES
        src/platform/mac/paths_mac.cpp
        src/platform/mac/startup_mac.cpp
        src/platform/mac/browser_host_mac.cpp
        src/platform/mac/nebula_window_mac.cpp
    )
elseif(OS_LINUX)
    set(NEBULA_PLATFORM_SOURCES
        src/platform/linux/paths_linux.cpp
        src/platform/linux/startup_linux.cpp
        src/platform/linux/browser_host_linux.cpp
        src/platform/linux/nebula_window_linux.cpp
    )
else()
    message(FATAL_ERROR "Unsupported platform.")
endif()

ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_RELEASE}" "${CEF_LIB_DEBUG}")

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

    target_link_libraries(${nebula_target} PRIVATE
        libcef_lib
        libcef_dll_wrapper
        ${CEF_STANDARD_LIBS}
    )

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
        set(NEBULA_APP "${CEF_TARGET_OUT_DIR}/${nebula_target}.app")
        COPY_MAC_FRAMEWORK(
            "${nebula_target}"
            "Chromium Embedded Framework"
            "${CEF_BINARY_DIR_RELEASE}"
            "${NEBULA_APP}/Contents/Frameworks"
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
