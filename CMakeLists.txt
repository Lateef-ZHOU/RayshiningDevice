cmake_minimum_required(VERSION 3.28)
project(RayshiningDevice LANGUAGES CXX)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.11.2 REQUIRED COMPONENTS Core Qml Quick GuiPrivate ShaderTools)
include(FetchContent)
if(NOT TARGET Boost::circular_buffer)
    find_package(Boost 1.88 QUIET CONFIG COMPONENTS circular_buffer)
    if(NOT TARGET Boost::circular_buffer)
        set(BOOST_INCLUDE_LIBRARIES ${BOOST_INCLUDE_LIBRARIES} circular_buffer)
        FetchContent_Declare(boost
            URL https://github.com/boostorg/boost/releases/download/boost-1.88.0/boost-1.88.0-cmake.tar.xz
            URL_HASH SHA256=f48b48390380cfb94a629872346e3a81370dc498896f16019ade727ab72eb1ec
            EXCLUDE_FROM_ALL)
        FetchContent_MakeAvailable(boost)
    endif()
endif()
if(NOT TARGET Lidar::HandleCore)
    set(HANDLECORE_WITH_SERIAL ON CACHE BOOL "Enable serial transport" FORCE)
    FetchContent_Declare(lidar_handle_core
        GIT_REPOSITORY https://github.com/Lateef-ZHOU/HandleCore.git
        GIT_TAG 379401c85f0da11adde9a2f6fde8fe767fbae707)
    FetchContent_MakeAvailable(lidar_handle_core)
endif()
if(NOT TARGET Lidar::Serial)
    message(FATAL_ERROR "Enable HANDLECORE_WITH_SERIAL before configuring HandleCore")
endif()
if(NOT TARGET Lidar::DeviceCore)
    FetchContent_Declare(lidar_device_core
        GIT_REPOSITORY https://github.com/Lateef-ZHOU/DeviceCore.git
        GIT_TAG 5c63ec5c501d83a15daa65a41dca438ee4ff330f)
    FetchContent_MakeAvailable(lidar_device_core)
endif()
if(NOT TARGET Lidar::ChartCore)
    # 尚未发布的图表接口按本地提交固定；正式发布后仅替换仓库地址。
    FetchContent_Declare(lidar_chartcore
        GIT_REPOSITORY "${CMAKE_CURRENT_SOURCE_DIR}/../ChartCore"
        GIT_TAG 5fd7a0663319e085089b6011510a6624ffb0708d)
    FetchContent_MakeAvailable(lidar_chartcore)
endif()
set(import_paths)
foreach(module Lidar::UICore Lidar::ChartCore)
    get_target_property(output ${module} QT_QML_MODULE_OUTPUT_DIRECTORY)
    get_filename_component(import_root "${output}/../.." ABSOLUTE)
    list(APPEND import_paths "${import_root}")
endforeach()
set(views qml/LaserModule.qml qml/LaserMonitor.qml)
foreach(view IN LISTS views)
    get_filename_component(name "${view}" NAME)
    set_source_files_properties("${view}" PROPERTIES QT_RESOURCE_ALIAS "${name}")
endforeach()
qt_add_qml_module(RayshiningDevice STATIC URI Lidar.Rayshining VERSION 1.0
    CLASS_NAME LidarRayshiningPlugin RESOURCE_PREFIX /qt/qml
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Lidar/Rayshining"
    DEPENDENCIES Lidar.UI Lidar.Charts IMPORT_PATH ${import_paths}
    SOURCES include/Rayshining/LaserHandle.h include/Rayshining/LaserDevice.h
    QML_FILES ${views})
add_library(Lidar::RayshiningDevice ALIAS RayshiningDevice)
set_target_properties(RayshiningDevice PROPERTIES AUTOMOC ON)
target_include_directories(RayshiningDevice PUBLIC include PRIVATE include/Rayshining)
target_compile_features(RayshiningDevice PUBLIC cxx_std_20)
target_compile_definitions(RayshiningDevice PUBLIC QT_NO_KEYWORDS)
target_link_libraries(RayshiningDevice PUBLIC Lidar::Serial Lidar::DeviceCore Boost::circular_buffer Lidar::ChartCore)
if(MSVC)
    target_compile_options(RayshiningDevice PRIVATE /W4 /utf-8)
    get_target_property(sources RayshiningDevice SOURCES)
    foreach(source IN LISTS sources)
        if(source MATCHES "/[.]rcc/qmlcache/")
            set_source_files_properties("${source}" PROPERTIES COMPILE_OPTIONS /wd4702)
        endif()
    endforeach()
else()
    target_compile_options(RayshiningDevice PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
