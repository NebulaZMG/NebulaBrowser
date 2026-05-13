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
        "Make sure the contents of the CEF binary distribution are inside thirdparty/cef."
    )
endif()

# ------------------------------------------------------------
# CEF setup
# ------------------------------------------------------------

list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")

find_package(CEF REQUIRED)

add_subdirectory(
    "${CEF_LIBCEF_DLL_WRAPPER_PATH}"
    "${CMAKE_BINARY_DIR}/libcef_dll_wrapper"
)

# ------------------------------------------------------------
# Nebula source files
# ------------------------------------------------------------

set(NEBULA_SOURCES
    app/main.cpp
    src/app/nebula_controller.cpp
    src/app/run.cpp
    src/browser/tab.cpp
    src/browser/tab_manager.cpp
    src/browser/url_utils.cpp
    src/cef/browser_client.cpp
    src/cef/nebula_app.cpp
    src/ui/paths.cpp
    src/window/nebula_window.cpp
)

add_executable(NebulaBrowser WIN32
    ${NEBULA_SOURCES}
)

SET_EXECUTABLE_TARGET_PROPERTIES(NebulaBrowser)

if(MSVC)
    set_property(TARGET NebulaBrowser PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

target_include_directories(NebulaBrowser PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CEF_ROOT}"
    "${CEF_ROOT}/include"
)

target_link_libraries(NebulaBrowser PRIVATE
    libcef_dll_wrapper
    ${CEF_STANDARD_LIBS}
)

# ------------------------------------------------------------
# Platform-specific CEF linking
# ------------------------------------------------------------

if(WIN32)
    target_link_libraries(NebulaBrowser PRIVATE
        "${CEF_ROOT}/Release/libcef.lib"
        dwmapi
    )

    target_compile_definitions(NebulaBrowser PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

# ------------------------------------------------------------
# Copy CEF runtime files after build
# ------------------------------------------------------------

if(WIN32)
    add_custom_command(TARGET NebulaBrowser POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CEF_ROOT}/Release"
            "$<TARGET_FILE_DIR:NebulaBrowser>"

        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CEF_ROOT}/Resources"
            "$<TARGET_FILE_DIR:NebulaBrowser>"

        COMMENT "Copying CEF runtime files..."
    )
endif()

# ------------------------------------------------------------
# Copy Nebula UI files after build
# ------------------------------------------------------------

add_custom_command(TARGET NebulaBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/ui"
        "$<TARGET_FILE_DIR:NebulaBrowser>/ui"

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:NebulaBrowser>/ui/assets"

    COMMENT "Copying Nebula UI files and assets..."
)