cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#




# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME 01MultiTasks)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)



# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)



# 1. 先用 GLOB_RECURSE 把所有的 .c 文件扫进 THIRDPARTY_SOURCES 列表中
# （强烈建议加上 ${CMAKE_CURRENT_SOURCE_DIR} 确保获取准确的绝对路径）
file(GLOB_RECURSE THIRDPARTY_SOURCES CONFIGURE_DEPENDS 
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/MyLib/*.c"
)
# 2. 从列表中精准剔除不需要的文件 
# （注意：这里的路径格式必须和上面扫描时的路径格式保持绝对一致！）
list(REMOVE_ITEM THIRDPARTY_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/MyLib/LCD/lcd_ex.c"
)



# 将扫描到的源文件列表加入到工程的编译源中
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${THIRDPARTY_SOURCES}
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

# 告诉编译器去哪里找头文件 (填的是目录路径，不是通配符)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ThirdParty/MyLib
    )

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
