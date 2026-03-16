cmake_minimum_required(VERSION 3.15)
project(GameApp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TOYLIB_PATH ToyLib)
set(TOYKIT_PATH ToyKit)
set(GAME_PATH  ToyGame)

find_package(OpenGL REQUIRED)

# ========================
# Vulkan (always link)
# ========================
# 3OS すべてで Vulkan loader/headers が入っている前提で構成する。
# - Windows: vcpkg (VulkanConfig.cmake)
# - Linux  : apt (FindVulkan.cmake で拾える)
# - macOS  : MoltenVK + vulkan-loader/headers を CMake が拾える状態（後述）
find_package(Vulkan REQUIRED)

# ToyLib のソース
file(GLOB_RECURSE TOYLIB_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/src/*.cpp
)

# ToyKit のソース
file(GLOB_RECURSE TOYKIT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/${TOYKIT_PATH}/src/*.cpp
)

# GameApp 以下のソース
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/${GAME_PATH}/*.cpp
)

set(SOURCES
    ${TOYLIB_SOURCES}
    ${TOYKIT_SOURCES}
    ${GAME_SOURCES}
)

add_executable(${PROJECT_NAME} ${SOURCES})

# ========================
# GLAD（組み込み）
# ========================
add_library(glad STATIC
    ${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/External/glad/src/glad.c
)

target_include_directories(glad PUBLIC
    ${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/External/glad/include
)

# ヘッダ（ToyLib / ToyKit / Game と External/json.hpp）
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/include
    ${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/External
    ${CMAKE_SOURCE_DIR}/${TOYKIT_PATH}/include
    ${CMAKE_SOURCE_DIR}/${GAME_PATH}
)

# ========================
# Linux: SDL3 fallback (system -> vendored source)
#  - Linux のみで有効化
# ========================
option(TOYLIB_LINUX_USE_VENDORED_SDL3 "Linux: use vendored SDL3 sources when system packages are not available" ON)

set(TOYLIB_EXTERNAL_DIR "${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/External" CACHE PATH "External dependencies root (ToyLib/External)")
set(TOYLIB_SDL3_SOURCE_DIR       "${TOYLIB_EXTERNAL_DIR}/SDL3"       CACHE PATH "Path to SDL3 source dir")
set(TOYLIB_SDL3_IMAGE_SOURCE_DIR "${TOYLIB_EXTERNAL_DIR}/SDL3_image" CACHE PATH "Path to SDL3_image source dir")
set(TOYLIB_SDL3_TTF_SOURCE_DIR   "${TOYLIB_EXTERNAL_DIR}/SDL3_ttf"   CACHE PATH "Path to SDL3_ttf source dir")

function(toy_linux_add_subdirectory_if_exists dir build_subdir out_var)
    if(EXISTS "${dir}/CMakeLists.txt")
        add_subdirectory("${dir}" "${build_subdir}" EXCLUDE_FROM_ALL)
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(toy_linux_find_or_vendored_sdl3)
    # ---- SDL3 ----
    find_package(SDL3 QUIET CONFIG)
    if(NOT SDL3_FOUND AND TOYLIB_LINUX_USE_VENDORED_SDL3)
        message(STATUS "Linux: SDL3 not found. Falling back to vendored source: ${TOYLIB_SDL3_SOURCE_DIR}")
        toy_linux_add_subdirectory_if_exists(
            "${TOYLIB_SDL3_SOURCE_DIR}"
            "${CMAKE_BINARY_DIR}/_deps/sdl3-build"
            _ok_sdl3
        )
        if(NOT _ok_sdl3 OR NOT TARGET SDL3::SDL3)
            message(FATAL_ERROR
                "Linux: SDL3 not found.\n"
                "  - Install system package (e.g. libsdl3-dev) OR\n"
                "  - Put SDL3 sources at: ${TOYLIB_SDL3_SOURCE_DIR}\n"
                "  - Or set -DTOYLIB_LINUX_USE_VENDORED_SDL3=OFF to disable fallback.\n"
            )
        endif()
    elseif(NOT SDL3_FOUND)
        message(FATAL_ERROR
            "Linux: SDL3 not found. Install system package (e.g. libsdl3-dev)\n"
            "or enable -DTOYLIB_LINUX_USE_VENDORED_SDL3=ON."
        )
    endif()

    # ---- SDL3_image ----
    find_package(SDL3_image QUIET CONFIG)
    if(NOT SDL3_image_FOUND AND TOYLIB_LINUX_USE_VENDORED_SDL3)
        message(STATUS "Linux: SDL3_image not found. Falling back to vendored source: ${TOYLIB_SDL3_IMAGE_SOURCE_DIR}")

        set(SDLIMAGE_VENDORED ON CACHE BOOL "Use vendored dependencies for SDL3_image" FORCE)

        toy_linux_add_subdirectory_if_exists(
            "${TOYLIB_SDL3_IMAGE_SOURCE_DIR}"
            "${CMAKE_BINARY_DIR}/_deps/sdl3_image-build"
            _ok_sdl3_image
        )
        if(NOT _ok_sdl3_image OR NOT TARGET SDL3_image::SDL3_image)
            message(FATAL_ERROR
                "Linux: SDL3_image not found.\n"
                "  - Install system package (e.g. libsdl3-image-dev) OR\n"
                "  - Put SDL3_image sources at: ${TOYLIB_SDL3_IMAGE_SOURCE_DIR}\n"
            )
        endif()
    elseif(NOT SDL3_image_FOUND)
        message(FATAL_ERROR
            "Linux: SDL3_image not found. Install system package (e.g. libsdl3-image-dev)\n"
            "or enable -DTOYLIB_LINUX_USE_VENDORED_SDL3=ON."
        )
    endif()

    # ---- SDL3_ttf ----
    find_package(SDL3_ttf QUIET CONFIG)
    if(NOT SDL3_ttf_FOUND AND TOYLIB_LINUX_USE_VENDORED_SDL3)
        message(STATUS "Linux: SDL3_ttf not found. Falling back to vendored source: ${TOYLIB_SDL3_TTF_SOURCE_DIR}")

        set(SDLTTF_VENDORED ON CACHE BOOL "Use vendored dependencies for SDL3_ttf" FORCE)

        toy_linux_add_subdirectory_if_exists(
            "${TOYLIB_SDL3_TTF_SOURCE_DIR}"
            "${CMAKE_BINARY_DIR}/_deps/sdl3_ttf-build"
            _ok_sdl3_ttf
        )
        if(NOT _ok_sdl3_ttf OR NOT TARGET SDL3_ttf::SDL3_ttf)
            message(FATAL_ERROR
                "Linux: SDL3_ttf not found.\n"
                "  - Install system package (e.g. libsdl3-ttf-dev) OR\n"
                "  - Put SDL3_ttf sources at: ${TOYLIB_SDL3_TTF_SOURCE_DIR}\n"
                "  Note: SDL3_ttf may require freetype/harfbuzz dev packages or vendored deps.\n"
            )
        endif()
    elseif(NOT SDL3_ttf_FOUND)
        message(FATAL_ERROR
            "Linux: SDL3_ttf not found. Install system package (e.g. libsdl3-ttf-dev)\n"
            "or enable -DTOYLIB_LINUX_USE_VENDORED_SDL3=ON."
        )
    endif()
endfunction()

#========================
# プラットフォーム別設定
#========================
if(WIN32)
    message(STATUS "Configuring for Windows build")

    find_package(SDL3 CONFIG REQUIRED)
    find_package(SDL3_image CONFIG REQUIRED)
    find_package(SDL3_ttf CONFIG REQUIRED)
    find_package(assimp CONFIG REQUIRED)
    find_package(OpenAL CONFIG REQUIRED)  # openal-soft (vcpkg) 前提

    # Vulkan は上で find_package(Vulkan REQUIRED) 済み
    # （vcpkg toolchain を使っている前提）

    # vcpkg
    set(VCPKG_ROOT "C:/Repos/vcpkg")
    set(VCPKG_TRIPLET "x64-windows")

    set(VCPKG_INC_DIR "${VCPKG_ROOT}/installed/${VCPKG_TRIPLET}/include")
    set(VCPKG_LIB_DIR "${VCPKG_ROOT}/installed/${VCPKG_TRIPLET}/lib")
    set(VCPKG_BIN_DIR "${VCPKG_ROOT}/installed/${VCPKG_TRIPLET}/bin")

    # mpg123
    find_path(MPG123_INCLUDE_DIR
        NAMES mpg123.h
        HINTS ${VCPKG_INC_DIR}
    )

    find_library(MPG123_LIBRARY
        NAMES mpg123
        HINTS ${VCPKG_LIB_DIR}
    )

    if(NOT MPG123_INCLUDE_DIR OR NOT MPG123_LIBRARY)
        message(FATAL_ERROR "mpg123 not found. Run: vcpkg install mpg123:x64-windows")
    endif()

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${VCPKG_INC_DIR}
        ${MPG123_INCLUDE_DIR}
    )

    target_link_libraries(${PROJECT_NAME}
        OpenGL32
        Vulkan::Vulkan
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_ttf::SDL3_ttf
        assimp::assimp
        OpenAL::OpenAL
        glad
        ${MPG123_LIBRARY}
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE SDL_MAIN_HANDLED)

    if (MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /utf-8 /MP)
        target_link_options(${PROJECT_NAME} PRIVATE /SUBSYSTEM:CONSOLE)
    endif()

    # DLL コピー
    # NOTE:
    #  Vulkan loader (vulkan-1.dll) は通常 OS/SDK 側に入っているが、
    #  “配布/別PCで確実に動かす” ならコピー戦略を別途検討（今は開発優先でOK）。
    set(DLL_FILES
        "${VCPKG_BIN_DIR}/SDL3.dll"
        "${VCPKG_BIN_DIR}/SDL3_image.dll"
        "${VCPKG_BIN_DIR}/jpeg62.dll"
        "${VCPKG_BIN_DIR}/libpng16.dll"
        "${VCPKG_BIN_DIR}/zlib1.dll"
        "${VCPKG_BIN_DIR}/SDL3_ttf.dll"
        "${VCPKG_BIN_DIR}/assimp-vc145-mt.dll"
        "${VCPKG_BIN_DIR}/freetype.dll"
        "${VCPKG_BIN_DIR}/bz2.dll"
        "${VCPKG_BIN_DIR}/openal32.dll"
        "${VCPKG_BIN_DIR}/mpg123.dll"
    )

    foreach(dll ${DLL_FILES})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${dll}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            COMMENT "Copying ${dll} to $<TARGET_FILE_DIR:${PROJECT_NAME}>"
        )
    endforeach()

elseif(UNIX AND NOT APPLE)
    message(STATUS "Configuring for Linux build (APT/system packages + SDL3 source fallback)")

    find_package(OpenGL REQUIRED)
    # Vulkan は上で find_package(Vulkan REQUIRED) 済み

    # SDL3: system -> source fallback（Linuxのみ）
    toy_linux_find_or_vendored_sdl3()

    # Assimp / OpenAL
    find_package(assimp REQUIRED)
    find_package(OpenAL REQUIRED)

    # mpg123
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(MPG123 QUIET IMPORTED_TARGET mpg123)
    endif()

    if(TARGET PkgConfig::MPG123)
        set(MPG123_TARGET PkgConfig::MPG123)
    else()
        find_path(MPG123_INCLUDE_DIR
            NAMES mpg123.h
            PATHS
                /usr/include
                /usr/local/include
                /usr/include/mpg123
                /usr/local/include/mpg123
        )

        find_library(MPG123_LIBRARY
            NAMES mpg123
            PATHS
                /usr/lib
                /usr/local/lib
                /usr/lib/x86_64-linux-gnu
                /usr/lib64
        )

        if(NOT MPG123_INCLUDE_DIR OR NOT MPG123_LIBRARY)
            message(FATAL_ERROR
                "mpg123 not found. Install: sudo apt install libmpg123-dev"
            )
        endif()

        add_library(mpg123::mpg123 UNKNOWN IMPORTED)
        set_target_properties(mpg123::mpg123 PROPERTIES
            IMPORTED_LOCATION "${MPG123_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${MPG123_INCLUDE_DIR}"
        )
        set(MPG123_TARGET mpg123::mpg123)
    endif()

    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            OpenGL::GL
            Vulkan::Vulkan
            SDL3::SDL3
            SDL3_image::SDL3_image
            SDL3_ttf::SDL3_ttf
            assimp::assimp
            OpenAL::OpenAL
            glad
            ${MPG123_TARGET}
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE SDL_MAIN_HANDLED)

elseif(APPLE)
    message(STATUS "Configuring for macOS build")

    find_package(OpenGL REQUIRED)
    # Vulkan は上で find_package(Vulkan REQUIRED) 済み

    # SDL3（Homebrew）
    find_package(SDL3        REQUIRED CONFIG HINTS /opt/homebrew/opt/sdl3)
    find_package(SDL3_image  REQUIRED CONFIG HINTS /opt/homebrew/opt/sdl3_image)
    find_package(SDL3_ttf    REQUIRED CONFIG HINTS /opt/homebrew/opt/sdl3_ttf)

    find_package(ASSIMP REQUIRED HINTS /opt/homebrew/opt/assimp)
    find_package(OpenAL REQUIRED HINTS /opt/homebrew/opt/openal-soft)

    # mpg123
    find_path(MPG123_INCLUDE_DIR
        NAMES mpg123.h
        HINTS
            /opt/homebrew/opt/mpg123/include
            /opt/homebrew/include
            /usr/local/include
    )

    find_library(MPG123_LIBRARY
        NAMES mpg123
        HINTS
            /opt/homebrew/opt/mpg123/lib
            /opt/homebrew/lib
            /usr/local/lib
    )

    if(NOT MPG123_INCLUDE_DIR OR NOT MPG123_LIBRARY)
        message(FATAL_ERROR "mpg123 not found. Please install with 'brew install mpg123'.")
    endif()

    target_include_directories(${PROJECT_NAME} PRIVATE
        /opt/homebrew/include
        ${ASSIMP_INCLUDE_DIRS}
        ${OPENAL_INCLUDE_DIR}
        ${MPG123_INCLUDE_DIR}
    )

    target_link_libraries(${PROJECT_NAME}
        OpenGL::GL
        Vulkan::Vulkan
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_ttf::SDL3_ttf
        assimp::assimp
        glad
        ${OPENAL_LIBRARY}
        ${MPG123_LIBRARY}
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "@executable_path/../lib;/opt/homebrew/lib"
    )

endif()

#========================
# 資材コピー（共通）
#========================

set(SOURCE_SHADER_DIR       "${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/Shaders")
set(SOURCE_SYSASSET_DIR     "${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/Assets")
set(SOURCE_ASSET_DIR        "${CMAKE_SOURCE_DIR}/${GAME_PATH}/Assets")
set(SOURCE_SETTINGS_DIR     "${CMAKE_SOURCE_DIR}/${GAME_PATH}/Settings")
set(SOURCE_SYSSETTINGS_DIR  "${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/Settings")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SOURCE_SHADER_DIR}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${TOYLIB_PATH}/Shaders"
    COMMENT "Copying Shaders to $<TARGET_FILE_DIR:${PROJECT_NAME}>/${TOYLIB_PATH}/Shaders"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SOURCE_SYSASSET_DIR}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${TOYLIB_PATH}/Assets"
    COMMENT "Copying System Assets to $<TARGET_FILE_DIR:${PROJECT_NAME}>/${TOYLIB_PATH}/Assets"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SOURCE_ASSET_DIR}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${GAME_PATH}/Assets"
    COMMENT "Copying Assets to $<TARGET_FILE_DIR:${PROJECT_NAME}>/${GAME_PATH}/Assets"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SOURCE_SETTINGS_DIR}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${GAME_PATH}/Settings"
    COMMENT "Copying Settings to $<TARGET_FILE_DIR:${PROJECT_NAME}>/${GAME_PATH}/Settings"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SOURCE_SYSSETTINGS_DIR}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${TOYLIB_PATH}/Settings"
    COMMENT "Copying Settings to $<TARGET_FILE_DIR:${PROJECT_NAME}>/${TOYLIB_PATH}/Settings"
)