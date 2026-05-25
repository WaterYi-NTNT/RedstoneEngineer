cmake_minimum_required(VERSION 3.22)
project(RedstoneEngineer VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release)
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -Wall" CACHE STRING "Debug compiler flags" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -Wall -DNDEBUG" CACHE STRING "Release compiler flags" FORCE)

if(WIN32 AND NOT DEFINED CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "E:/Qt/6.11.0/mingw_64")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)

set(PLATFORM_LIBS)
if(WIN32)
    list(APPEND PLATFORM_LIBS opengl32)
elseif(APPLE)
    find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
    list(APPEND PLATFORM_LIBS ${OPENGL_FRAMEWORK})
elseif(UNIX)
    find_package(OpenGL REQUIRED)
    list(APPEND PLATFORM_LIBS OpenGL::GL)
endif()

set(PROJECT_SOURCES
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp

    src/core/Block.h
    src/core/VoxelWorld.h
    src/core/VoxelWorld.cpp

    src/core/BlockModel.h
    src/core/BlockModelLoader.h
    src/core/BlockModelLoader.cpp
    src/core/BlockStateLoader.h
    src/core/BlockStateLoader.cpp
    src/core/BuiltinModels.h
    src/core/BuiltinModels.cpp

    src/editor2d/GridView.h
    src/editor2d/GridView.cpp
    src/editor2d/GridScene.h
    src/editor2d/GridScene.cpp

    src/palette/PaletteItem.h
    src/palette/PaletteItem.cpp
    src/palette/PaletteGroup.h
    src/palette/PaletteGroup.cpp
    src/palette/BlockPalette.h
    src/palette/BlockPalette.cpp

    src/preview3d/Camera.h
    src/preview3d/Camera.cpp
    src/preview3d/VoxelMesh.h
    src/preview3d/VoxelMesh.cpp
    src/preview3d/VoxelRenderer.h
    src/preview3d/VoxelRenderer.cpp
    src/preview3d/ModelIconRenderer.h
    src/preview3d/ModelIconRenderer.cpp

    src/sim/SimFlags.h
    src/sim/TickScheduler.h
    src/sim/SimEngine.h
    src/sim/SimEngine.cpp
    src/sim/RedstoneLogic.h
    src/sim/RedstoneLogic.cpp
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc")
    list(APPEND PROJECT_SOURCES resources/resources.qrc)
endif()

set(EXECUTABLE_OPTIONS)
if(WIN32)
    list(APPEND EXECUTABLE_OPTIONS WIN32)
endif()

qt_add_executable(RedstoneEngineer ${EXECUTABLE_OPTIONS} ${PROJECT_SOURCES})

add_custom_command(TARGET RedstoneEngineer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:RedstoneEngineer>/data"
    COMMENT "Copying data directory to build output..."
)

target_link_libraries(RedstoneEngineer PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets
    ${PLATFORM_LIBS}
)

target_include_directories(RedstoneEngineer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor2d
    ${CMAKE_CURRENT_SOURCE_DIR}/src/palette
    ${CMAKE_CURRENT_SOURCE_DIR}/src/preview3d
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sim
)

set(REDSTONEENGINEER_APPDIR "lib/redstoneengineer")
set(REDSTONEENGINEER_RUNTIME_DIR "/usr/${REDSTONEENGINEER_APPDIR}")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/redstoneengineer.in"
    "${CMAKE_CURRENT_BINARY_DIR}/redstoneengineer"
    @ONLY
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/redstoneengineer.desktop.in"
    "${CMAKE_CURRENT_BINARY_DIR}/redstoneengineer.desktop"
    @ONLY
)

install(TARGETS RedstoneEngineer
    RUNTIME DESTINATION "${REDSTONEENGINEER_APPDIR}"
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data"
    DESTINATION "${REDSTONEENGINEER_APPDIR}"
)

install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/redstoneengineer"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/redstoneengineer.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
)

set(CPACK_PACKAGE_NAME "redstoneengineer")
set(CPACK_PACKAGE_VENDOR "RedstoneEngineer")
set(CPACK_PACKAGE_CONTACT "RedstoneEngineer Maintainers")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Minecraft redstone circuit simulator built with Qt 6")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "RedstoneEngineer Maintainers")
set(CPACK_DEBIAN_PACKAGE_SECTION "electronics")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libgcc-s1, libstdc++6, libopengl0, libqt6core6 (>= 6.4), libqt6gui6 (>= 6.4), libqt6widgets6 (>= 6.4), libqt6opengl6 (>= 6.4), libqt6openglwidgets6 (>= 6.4)")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

set(REDSTONEENGINEER_DEB_ARCHITECTURE "amd64" CACHE STRING "Debian package architecture")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${REDSTONEENGINEER_DEB_ARCHITECTURE}")

add_custom_target(package-deb-amd64
    COMMAND ${CMAKE_CPACK_COMMAND} -G DEB
        -D CPACK_DEBIAN_PACKAGE_ARCHITECTURE=amd64
        --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
    DEPENDS RedstoneEngineer
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Building amd64 Debian package"
)

add_custom_target(package-deb-aarch64
    COMMAND ${CMAKE_CPACK_COMMAND} -G DEB
        -D CPACK_DEBIAN_PACKAGE_ARCHITECTURE=arm64
        --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
    DEPENDS RedstoneEngineer
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Building aarch64/arm64 Debian package"
)

add_custom_target(package-deb-all
    COMMAND ${CMAKE_CPACK_COMMAND} -G DEB
        -D CPACK_DEBIAN_PACKAGE_ARCHITECTURE=amd64
        --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
    COMMAND ${CMAKE_CPACK_COMMAND} -G DEB
        -D CPACK_DEBIAN_PACKAGE_ARCHITECTURE=arm64
        --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
    DEPENDS RedstoneEngineer
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Building amd64 and aarch64/arm64 Debian packages"
)

include(CPack)
