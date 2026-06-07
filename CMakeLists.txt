cmake_minimum_required(VERSION 3.22)
project(RedstoneEngineer VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT DEFINED CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "E:/Qt/6.11.0/mingw_64")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGLWidgets)

if(NOT ZLIB_INCLUDE_DIR)
    find_path(ZLIB_INCLUDE_DIR zlib.h
        HINTS
            "${Qt6_DIR}/../../../Tools/mingw1310_64/x86_64-w64-mingw32/include"
            "E:/Qt/Tools/mingw1310_64/x86_64-w64-mingw32/include"
        NO_DEFAULT_PATH
    )
endif()

if(NOT ZLIB_LIBRARY)
    find_library(ZLIB_LIBRARY z
        HINTS
            "${Qt6_DIR}/../../../Tools/mingw1310_64/x86_64-w64-mingw32/lib"
            "E:/Qt/Tools/mingw1310_64/x86_64-w64-mingw32/lib"
        NO_DEFAULT_PATH
    )
endif()

find_package(ZLIB REQUIRED)

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
    src/editor2d/EditorCommand.h
    src/editor2d/EditorCommand.cpp
    src/editor2d/CommandHistory.h
    src/editor2d/CommandHistory.cpp

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

    src/io/LitematicExporter.h
    src/io/LitematicExporter.cpp
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc")
    list(APPEND PROJECT_SOURCES resources/resources.qrc)
endif()

qt_add_executable(RedstoneEngineer WIN32 ${PROJECT_SOURCES})

add_custom_command(TARGET RedstoneEngineer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:RedstoneEngineer>/data"
    COMMENT "Copying data directory to build output..."
)

target_link_libraries(RedstoneEngineer PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGLWidgets opengl32
    ZLIB::ZLIB
)

target_include_directories(RedstoneEngineer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor2d
    ${CMAKE_CURRENT_SOURCE_DIR}/src/palette
    ${CMAKE_CURRENT_SOURCE_DIR}/src/preview3d
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sim
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io
)