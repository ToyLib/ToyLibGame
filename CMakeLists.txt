cmake_minimum_required(VERSION 3.15)

project(GameApp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# Project paths
# ============================================================

set(TOYLIB_PATH ToyLib)
set(TOYKIT_PATH ToyKit)
set(GAME_PATH   ToyGame)

# ============================================================
# Windows / vcpkg settings
# ============================================================

if(WIN32)

    set(VCPKG_ROOT "C:/tools/vcpkg")
    set(VCPKG_TRIPLET "x64-windows")

    set(
        VCPKG_INC_DIR
        "${VCPKG_ROOT}/installed/${VCPKG_TRIPLET}/include"
    )

    set(
        VCPKG_LIB_DIR
        "${VCPKG_ROOT}/installed/${VCPKG_TRIPLET}/lib"
    )

    set(
        VCPKG_BIN_DIR
        "${VCPKG_ROOT}/installed/${VCPKG_TRIPLET}/bin"
    )

endif()

# ============================================================
# OpenGL
# ============================================================

find_package(OpenGL REQUIRED)

# ============================================================
# Vulkan
# ============================================================

find_package(Vulkan REQUIRED)

# ============================================================
# Source files
# ============================================================

# ToyLib
file(
    GLOB_RECURSE TOYLIB_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/src/*.cpp"
)

# ToyKit
file(
    GLOB_RECURSE TOYKIT_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/${TOYKIT_PATH}/src/*.cpp"
)

# Game
file(
    GLOB_RECURSE GAME_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/${GAME_PATH}/*.cpp"
)

set(
    SOURCES
    ${TOYLIB_SOURCES}
    ${TOYKIT_SOURCES}
    ${GAME_SOURCES}
)

# ============================================================
# GLAD
# ============================================================

add_library(
    glad
    STATIC
    "${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/External/glad/src/glad.c"
)

target_include_directories(
    glad
    PUBLIC
    "${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/External/glad/include"
)

# ============================================================
# stb_vorbis
# ============================================================

add_library(
    stb_vorbis
    STATIC
    "${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/External/stb_vorbis.c"
)

# ============================================================
# Game executable
# ============================================================

add_executable(
    ${PROJECT_NAME}
    ${SOURCES}
)

# ============================================================
# Include directories
# ============================================================

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
    "${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/include"
    "${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/External"
    "${CMAKE_SOURCE_DIR}/${TOYKIT_PATH}/include"
    "${CMAKE_SOURCE_DIR}/${GAME_PATH}"
)

# ============================================================
# Linux SDL3 fallback
# ============================================================

option(
    TOYLIB_LINUX_USE_VENDORED_SDL3
    "Linux: use vendored SDL3 sources when system packages are not available"
    ON
)

set(
    TOYLIB_EXTERNAL_DIR
    "${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/External"
    CACHE PATH
    "External dependencies root (ToyLib/External)"
)

set(
    TOYLIB_SDL3_SOURCE_DIR
    "${TOYLIB_EXTERNAL_DIR}/SDL3"
    CACHE PATH
    "Path to SDL3 source dir"
)

set(
    TOYLIB_SDL3_IMAGE_SOURCE_DIR
    "${TOYLIB_EXTERNAL_DIR}/SDL3_image"
    CACHE PATH
    "Path to SDL3_image source dir"
)

set(
    TOYLIB_SDL3_TTF_SOURCE_DIR
    "${TOYLIB_EXTERNAL_DIR}/SDL3_ttf"
    CACHE PATH
    "Path to SDL3_ttf source dir"
)

function(
    toy_linux_add_subdirectory_if_exists
    dir
    build_subdir
    out_var
)

    if(EXISTS "${dir}/CMakeLists.txt")

        add_subdirectory(
            "${dir}"
            "${build_subdir}"
            EXCLUDE_FROM_ALL
        )

        set(${out_var} TRUE PARENT_SCOPE)

    else()

        set(${out_var} FALSE PARENT_SCOPE)

    endif()

endfunction()

function(toy_linux_find_or_vendored_sdl3)

    # --------------------------------------------------------
    # SDL3
    # --------------------------------------------------------

    find_package(SDL3 QUIET CONFIG)

    if(NOT SDL3_FOUND AND TOYLIB_LINUX_USE_VENDORED_SDL3)

        message(
            STATUS
            "Linux: SDL3 not found. Falling back to vendored source: ${TOYLIB_SDL3_SOURCE_DIR}"
        )

        toy_linux_add_subdirectory_if_exists(
            "${TOYLIB_SDL3_SOURCE_DIR}"
            "${CMAKE_BINARY_DIR}/_deps/sdl3-build"
            _ok_sdl3
        )

        if(NOT _ok_sdl3 OR NOT TARGET SDL3::SDL3)

            message(
                FATAL_ERROR
                "Linux: SDL3 not found.\n"
                "  - Install system package (e.g. libsdl3-dev) OR\n"
                "  - Put SDL3 sources at: ${TOYLIB_SDL3_SOURCE_DIR}\n"
                "  - Or set -DTOYLIB_LINUX_USE_VENDORED_SDL3=OFF."
            )

        endif()

    elseif(NOT SDL3_FOUND)

        message(
            FATAL_ERROR
            "Linux: SDL3 not found.\n"
            "Install system package or enable vendored SDL3."
        )

    endif()

    # --------------------------------------------------------
    # SDL3_image
    # --------------------------------------------------------

    find_package(SDL3_image QUIET CONFIG)

    if(
        NOT SDL3_image_FOUND
        AND TOYLIB_LINUX_USE_VENDORED_SDL3
    )

        message(
            STATUS
            "Linux: SDL3_image not found. Falling back to vendored source: ${TOYLIB_SDL3_IMAGE_SOURCE_DIR}"
        )

        set(
            SDLIMAGE_VENDORED
            ON
            CACHE BOOL
            "Use vendored dependencies for SDL3_image"
            FORCE
        )

        toy_linux_add_subdirectory_if_exists(
            "${TOYLIB_SDL3_IMAGE_SOURCE_DIR}"
            "${CMAKE_BINARY_DIR}/_deps/sdl3_image-build"
            _ok_sdl3_image
        )

        if(
            NOT _ok_sdl3_image
            OR NOT TARGET SDL3_image::SDL3_image
        )

            message(
                FATAL_ERROR
                "Linux: SDL3_image not found.\n"
                "  - Install system package OR\n"
                "  - Put SDL3_image sources at: ${TOYLIB_SDL3_IMAGE_SOURCE_DIR}"
            )

        endif()

    elseif(NOT SDL3_image_FOUND)

        message(
            FATAL_ERROR
            "Linux: SDL3_image not found."
        )

    endif()

    # --------------------------------------------------------
    # SDL3_ttf
    # --------------------------------------------------------

    find_package(SDL3_ttf QUIET CONFIG)

    if(
        NOT SDL3_ttf_FOUND
        AND TOYLIB_LINUX_USE_VENDORED_SDL3
    )

        message(
            STATUS
            "Linux: SDL3_ttf not found. Falling back to vendored source: ${TOYLIB_SDL3_TTF_SOURCE_DIR}"
        )

        set(
            SDLTTF_VENDORED
            ON
            CACHE BOOL
            "Use vendored dependencies for SDL3_ttf"
            FORCE
        )

        toy_linux_add_subdirectory_if_exists(
            "${TOYLIB_SDL3_TTF_SOURCE_DIR}"
            "${CMAKE_BINARY_DIR}/_deps/sdl3_ttf-build"
            _ok_sdl3_ttf
        )

        if(
            NOT _ok_sdl3_ttf
            OR NOT TARGET SDL3_ttf::SDL3_ttf
        )

            message(
                FATAL_ERROR
                "Linux: SDL3_ttf not found.\n"
                "  - Install system package OR\n"
                "  - Put SDL3_ttf sources at: ${TOYLIB_SDL3_TTF_SOURCE_DIR}"
            )

        endif()

    elseif(NOT SDL3_ttf_FOUND)

        message(
            FATAL_ERROR
            "Linux: SDL3_ttf not found."
        )

    endif()

endfunction()

# ============================================================
# Platform settings
# ============================================================

if(WIN32)

    # ========================================================
    # Windows
    # ========================================================

    message(STATUS "Configuring for Windows build")

    find_package(SDL3 CONFIG REQUIRED)
    find_package(SDL3_image CONFIG REQUIRED)
    find_package(SDL3_ttf CONFIG REQUIRED)
    find_package(assimp CONFIG REQUIRED)
    find_package(OpenAL CONFIG REQUIRED)

    target_include_directories(
        ${PROJECT_NAME}
        PRIVATE
        "${VCPKG_INC_DIR}"
    )

    target_link_libraries(
        ${PROJECT_NAME}
        PRIVATE
        OpenGL32
        Vulkan::Vulkan
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_ttf::SDL3_ttf
        assimp::assimp
        OpenAL::OpenAL
        glad
        stb_vorbis
    )

    target_compile_definitions(
        ${PROJECT_NAME}
        PRIVATE
        SDL_MAIN_HANDLED
    )

    if(MSVC)

        target_compile_options(
            ${PROJECT_NAME}
            PRIVATE
            /utf-8
            /MP
        )

        target_link_options(
            ${PROJECT_NAME}
            PRIVATE
            /SUBSYSTEM:CONSOLE
        )

    endif()

    # --------------------------------------------------------
    # DLL copy
    #
    # DLL は実行ファイルが再リンクされたときにコピーする。
    # --------------------------------------------------------

    set(
        DLL_FILES

        "${VCPKG_BIN_DIR}/SDL3.dll"
        "${VCPKG_BIN_DIR}/SDL3_image.dll"
        "${VCPKG_BIN_DIR}/jpeg62.dll"
        "${VCPKG_BIN_DIR}/libpng16.dll"
        "${VCPKG_BIN_DIR}/z.dll"
        "${VCPKG_BIN_DIR}/SDL3_ttf.dll"

        "${VCPKG_BIN_DIR}/assimp-vc145-mt.dll"
        "${VCPKG_BIN_DIR}/fmt.dll"

        "${VCPKG_BIN_DIR}/freetype.dll"
        "${VCPKG_BIN_DIR}/bz2.dll"

        "${VCPKG_BIN_DIR}/openal32.dll"
    )

    foreach(dll ${DLL_FILES})

        if(EXISTS "${dll}")

            add_custom_command(
                TARGET ${PROJECT_NAME}
                POST_BUILD

                COMMAND
                    ${CMAKE_COMMAND}
                    -E copy_if_different
                    "${dll}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"

                COMMENT
                    "Copying ${dll}"

                VERBATIM
            )

        endif()

    endforeach()

elseif(UNIX AND NOT APPLE)

    # ========================================================
    # Linux
    # ========================================================

    message(
        STATUS
        "Configuring for Linux build (APT/system packages + SDL3 source fallback)"
    )

    toy_linux_find_or_vendored_sdl3()

    find_package(assimp REQUIRED)
    find_package(OpenAL REQUIRED)

    target_link_libraries(
        ${PROJECT_NAME}
        PRIVATE
        OpenGL::GL
        Vulkan::Vulkan
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_ttf::SDL3_ttf
        assimp::assimp
        OpenAL::OpenAL
        glad
        stb_vorbis
    )

    target_compile_definitions(
        ${PROJECT_NAME}
        PRIVATE
        SDL_MAIN_HANDLED
    )

elseif(APPLE)

    # ========================================================
    # macOS
    # ========================================================

    message(STATUS "Configuring for macOS build")

    find_package(
        SDL3
        REQUIRED
        CONFIG
        HINTS
        /opt/homebrew/opt/sdl3
        /usr/local/opt/sdl3
    )

    find_package(
        SDL3_image
        REQUIRED
        CONFIG
        HINTS
        /opt/homebrew/opt/sdl3_image
        /usr/local/opt/sdl3_image
    )

    find_package(
        SDL3_ttf
        REQUIRED
        CONFIG
        HINTS
        /opt/homebrew/opt/sdl3_ttf
        /usr/local/opt/sdl3_ttf
    )

    find_package(
        ASSIMP
        REQUIRED
        HINTS
        /opt/homebrew/opt/assimp
        /usr/local/opt/assimp
    )

    find_package(
        OpenAL
        REQUIRED
        HINTS
        /opt/homebrew/opt/openal-soft
        /usr/local/opt/openal-soft
    )

    target_include_directories(
        ${PROJECT_NAME}
        PRIVATE

        /opt/homebrew/include
        /usr/local/include

        ${ASSIMP_INCLUDE_DIRS}
        ${OPENAL_INCLUDE_DIR}
        ${MPG123_INCLUDE_DIR}
    )

    target_link_libraries(
        ${PROJECT_NAME}
        PRIVATE

        OpenGL::GL
        Vulkan::Vulkan

        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_ttf::SDL3_ttf

        assimp::assimp

        glad
        stb_vorbis

        ${OPENAL_LIBRARY}
    )

    set_target_properties(
        ${PROJECT_NAME}

        PROPERTIES

        BUILD_WITH_INSTALL_RPATH TRUE

        INSTALL_RPATH
        "@executable_path/../lib;/opt/homebrew/lib;/usr/local/lib"
    )

endif()

# ============================================================
# Vulkan shader compiler
# GLSL -> SPIR-V
# ============================================================

set(
    VK_SHADER_SRC_DIR
    "${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/Shaders/VK/src"
)

set(
    VK_SHADER_SPV_DIR
    "${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/Shaders/VK/spv"
)

# ------------------------------------------------------------
# Find glslc
# ------------------------------------------------------------

if(WIN32)

    find_program(
        GLSLC_EXECUTABLE

        NAMES
        glslc.exe
        glslc

        HINTS
        "${VCPKG_ROOT}/installed/${VCPKG_TRIPLET}/tools/shaderc"
        "${VCPKG_BIN_DIR}"
    )

elseif(APPLE)

    find_program(
        GLSLC_EXECUTABLE

        NAMES
        glslc

        HINTS
        /opt/homebrew/bin
        /usr/local/bin
    )

else()

    find_program(
        GLSLC_EXECUTABLE

        NAMES
        glslc

        HINTS
        /usr/bin
        /usr/local/bin
    )

endif()

if(NOT GLSLC_EXECUTABLE)

    message(
        FATAL_ERROR

        "glslc was not found.\n"
        "\n"
        "Windows:\n"
        "  vcpkg install shaderc:x64-windows\n"
        "\n"
        "macOS:\n"
        "  brew install shaderc\n"
        "\n"
        "Linux:\n"
        "  Install glslc / shaderc package.\n"
    )

endif()

message(
    STATUS
    "Vulkan shader compiler: ${GLSLC_EXECUTABLE}"
)

# ------------------------------------------------------------
# Shader source list
# ------------------------------------------------------------

file(
    GLOB VK_SHADER_SOURCES

    CONFIGURE_DEPENDS

    "${VK_SHADER_SRC_DIR}/*.vert"
    "${VK_SHADER_SRC_DIR}/*.frag"
    "${VK_SHADER_SRC_DIR}/*.comp"
)

set(VK_SHADER_SPV_FILES)

foreach(SHADER ${VK_SHADER_SOURCES})

    get_filename_component(
        SHADER_NAME
        "${SHADER}"
        NAME
    )

    set(
        SPV_FILE
        "${VK_SHADER_SPV_DIR}/${SHADER_NAME}.spv"
    )

    add_custom_command(
        OUTPUT
        "${SPV_FILE}"

        COMMAND
            ${CMAKE_COMMAND}
            -E make_directory
            "${VK_SHADER_SPV_DIR}"

        COMMAND
            "${GLSLC_EXECUTABLE}"
            "${SHADER}"
            -o
            "${SPV_FILE}"

        DEPENDS
            "${SHADER}"

        COMMENT
            "Compiling Vulkan shader: ${SHADER_NAME}"

        VERBATIM
    )

    list(
        APPEND
        VK_SHADER_SPV_FILES
        "${SPV_FILE}"
    )

endforeach()

# ------------------------------------------------------------
# Vulkan shader target
# ------------------------------------------------------------

add_custom_target(
    VulkanShaders
    DEPENDS
    ${VK_SHADER_SPV_FILES}
)

# GameApp requires current Vulkan shaders.
add_dependencies(
    ${PROJECT_NAME}
    VulkanShaders
)

# ============================================================
# Runtime resources
# ============================================================

set(
    SOURCE_SHADER_DIR
    "${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/Shaders"
)

set(
    SOURCE_SYSASSET_DIR
    "${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/Assets"
)

set(
    SOURCE_ASSET_DIR
    "${CMAKE_SOURCE_DIR}/${GAME_PATH}/Assets"
)

set(
    SOURCE_SETTINGS_DIR
    "${CMAKE_SOURCE_DIR}/${GAME_PATH}/Settings"
)

set(
    SOURCE_SYSSETTINGS_DIR
    "${CMAKE_SOURCE_DIR}/${TOYLIB_PATH}/Settings"
)

# ------------------------------------------------------------
# Runtime output directory
#
# Windows / Visual Studio:
#
#   build/Debug
#   build/Release
#
# Linux / macOS:
#
#   build
# ------------------------------------------------------------

if(WIN32)

    set(
        RUNTIME_OUTPUT_DIR
        "${CMAKE_BINARY_DIR}/$<CONFIG>"
    )

else()

    set(
        RUNTIME_OUTPUT_DIR
        "${CMAKE_BINARY_DIR}"
    )

endif()

# ============================================================
# PrepareRun
#
# This is the normal "ready to launch" target.
#
# 1. VulkanShaders is checked through GameApp dependency.
# 2. GameApp is rebuilt only when required.
# 3. Runtime resources are always copied.
#
# ALL means normal `cmake --build build` also performs this step.
# ============================================================

add_custom_target(
    PrepareRun
    ALL

    # --------------------------------------------------------
    # Shaders
    # --------------------------------------------------------

    COMMAND
        ${CMAKE_COMMAND}
        -E copy_directory
        "${SOURCE_SHADER_DIR}"
        "${RUNTIME_OUTPUT_DIR}/${TOYLIB_PATH}/Shaders"

    # --------------------------------------------------------
    # ToyLib Assets
    # --------------------------------------------------------

    COMMAND
        ${CMAKE_COMMAND}
        -E copy_directory
        "${SOURCE_SYSASSET_DIR}"
        "${RUNTIME_OUTPUT_DIR}/${TOYLIB_PATH}/Assets"

    # --------------------------------------------------------
    # Game Assets
    # --------------------------------------------------------

    COMMAND
        ${CMAKE_COMMAND}
        -E copy_directory
        "${SOURCE_ASSET_DIR}"
        "${RUNTIME_OUTPUT_DIR}/${GAME_PATH}/Assets"

    # --------------------------------------------------------
    # Game Settings
    # --------------------------------------------------------

    COMMAND
        ${CMAKE_COMMAND}
        -E copy_directory
        "${SOURCE_SETTINGS_DIR}"
        "${RUNTIME_OUTPUT_DIR}/${GAME_PATH}/Settings"

    # --------------------------------------------------------
    # ToyLib Settings
    # --------------------------------------------------------

    COMMAND
        ${CMAKE_COMMAND}
        -E copy_directory
        "${SOURCE_SYSSETTINGS_DIR}"
        "${RUNTIME_OUTPUT_DIR}/${TOYLIB_PATH}/Settings"

    DEPENDS
        ${PROJECT_NAME}

    COMMENT
        "Preparing GameApp runtime resources"

    VERBATIM
)