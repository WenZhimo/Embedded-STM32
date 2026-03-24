# ThirdParty.cmake

# 1. 设置搜索基准目录
set(USER_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty")

# 检查目录是否存在，防止新项目报错
if(EXISTS "${USER_LIB_DIR}")
    message(STATUS "Found ThirdParty directory, configuring...")

    # 2. 递归扫描所有 .c 文件
    file(GLOB_RECURSE USER_SOURCES CONFIGURE_DEPENDS 
        "${USER_LIB_DIR}/*.c"
    )

    # 3. 精准剔除（如果新项目没这个文件，list 会自动忽略，不会报错）
    list(REMOVE_ITEM USER_SOURCES 
        "${USER_LIB_DIR}/MyLib/LCD/lcd_ex.c"
    )

    # 4. 自动提取所有包含 .h 文件的文件夹路径
    file(GLOB_RECURSE USER_HEADERS CONFIGURE_DEPENDS "${USER_LIB_DIR}/*.h")
    set(USER_INC_DIRS "")
    foreach(h_file ${USER_HEADERS})
        get_filename_component(h_dir ${h_file} DIRECTORY)
        list(APPEND USER_INC_DIRS ${h_dir})
    endforeach()
    list(REMOVE_DUPLICATES USER_INC_DIRS)

    # 5. 直接将资源注入到主目标中
    # 注意：这里的 ${CMAKE_PROJECT_NAME} 必须在 include 之前已定义
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${USER_SOURCES})
    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${USER_INC_DIRS})

    message(STATUS "ThirdParty: Added ${USER_SOURCES}")
else()
    message(WARNING "ThirdParty directory NOT found at: ${USER_LIB_DIR}")
endif()