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

    # 3. 精准剔除
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
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${USER_SOURCES})
    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${USER_INC_DIRS})

    message(STATUS "ThirdParty: Added ${USER_SOURCES}")

    # =======================================================
    # 6. 新增：处理 libsodium 静态库链接
    # =======================================================
    set(LIBSODIUM_LIB_PATH "${USER_LIB_DIR}/libsodium/lib/libsodium.a")
    
    # 检查 .a 文件是否存在
    if(EXISTS "${LIBSODIUM_LIB_PATH}")
        # 声明一个导入的静态库
        add_library(sodium STATIC IMPORTED)
        # 指定该库的物理路径
        set_target_properties(sodium PROPERTIES IMPORTED_LOCATION "${LIBSODIUM_LIB_PATH}")
        # 将静态库链接到主工程
        target_link_libraries(${CMAKE_PROJECT_NAME} sodium)
        message(STATUS "ThirdParty: Linked libsodium.a successfully")
    else()
        message(WARNING "ThirdParty: libsodium.a not found at ${LIBSODIUM_LIB_PATH}")
    endif()
    # =======================================================

else()
    message(WARNING "ThirdParty directory NOT found at: ${USER_LIB_DIR}")
endif()