cmake_minimum_required(VERSION 3.28)
project(SaveExtractor)

set(CMAKE_CXX_STANDARD 20)

# Include JE2BE-Core.
add_subdirectory("je2be-core")

# Include bit7z.

# BIT7Z_AUTO_FORMAT is required for BitFormat::Auto.
# This means we do not need the user to specify the archive type.
set(BIT7Z_AUTO_FORMAT true)
set(BIT7Z_USE_NATIVE_STRING true)
add_subdirectory("bit7z")

add_executable(${PROJECT_NAME}
        save_extractor.cpp
        unicode.hpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE je2be)
target_link_libraries(${PROJECT_NAME} PRIVATE bit7z)

find_package(cxxopts CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE cxxopts::cxxopts)

# Termcolor library
find_path(TERMCOLOR_INCLUDE_DIRS "termcolor/termcolor.hpp")
target_include_directories(${PROJECT_NAME} PRIVATE ${TERMCOLOR_INCLUDE_DIRS})

# copy 7z.dll from "dll" folder, into release dir
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/dll/7z.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMENT "Copying 7z.dll to the output directory"
)
