cmake_minimum_required(VERSION 3.28)
project(X360MSE)

# Use C++ 20.
set(CMAKE_CXX_STANDARD 20)

if (MSVC)
    # Using MSVC, set maximum optimization (/Ox) and Whole Program Optimization (/GL).
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ox /GL")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
else()
    # For other compilers, set to use the highest optimization level (-O3)
    # and enable Link Time Optimization (-flto).
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto")
endif()

# Include pbar.
# Disable pbar examples.
set(PBAR_INCLUDE_EXAMPLES OFF)
add_subdirectory("pbar")

# Include je2be-core.
add_subdirectory("je2be-core")

# BIT7Z_AUTO_FORMAT is required for BitFormat::Auto.
# This means we do not need the user to specify the archive type.
set(BIT7Z_AUTO_FORMAT true)
set(BIT7Z_USE_NATIVE_STRING true)

# Include bit7z.
add_subdirectory("bit7z")

# Add project files.
add_executable(${PROJECT_NAME}
        src/main.cpp
        src/unicode.hpp
)

# Link the output of je2be-ore.
target_link_libraries(${PROJECT_NAME} PRIVATE je2be)

# Link the output of bit7z.
target_link_libraries(${PROJECT_NAME} PRIVATE bit7z)

# Link cxxopts.
find_package(cxxopts CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE cxxopts::cxxopts)

# Include termcolor.
find_path(TERMCOLOR_INCLUDE_DIRS "termcolor/termcolor.hpp")
target_include_directories(${PROJECT_NAME} PRIVATE ${TERMCOLOR_INCLUDE_DIRS})

# Include pbar.
target_include_directories(${PROJECT_NAME} PRIVATE ${pbar_SOURCE_DIR})

# Copy '7z.dll' into the output directory.
# You must supply '7z.dll' into 'dll/' yourself.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/dll/7z.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMENT "Copying '7z.dll' to the output directory"
)
