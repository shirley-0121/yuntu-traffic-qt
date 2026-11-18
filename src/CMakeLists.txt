cmake_minimum_required(VERSION 3.21)
project(YuntuTrafficQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(YuntuTrafficQt
    main.cpp
    mainwindow.cpp
    mainwindow.h
    transit_core.cpp
    transit_core.h
)

target_link_libraries(YuntuTrafficQt PRIVATE Qt6::Widgets)

if (WIN32)
    set_target_properties(YuntuTrafficQt PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if (MSVC)
    target_compile_options(YuntuTrafficQt PRIVATE /utf-8)
endif()

add_custom_command(TARGET YuntuTrafficQt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/data
            $<TARGET_FILE_DIR:YuntuTrafficQt>/data
)
