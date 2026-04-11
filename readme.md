
# STM32 学习项目展示
![Stone Badge](https://stone.professorlee.work/api/stone/WenZhimo/Embedded-STM32)
## 学习笔记查看
对于`21KaoQingSystem_Server`及以前的的项目，学习笔记包含在对应外设代码文件中。

对于`22STM32G4_GPIO_MOTOR`及以后的项目，学习笔记可在[语雀知识库-嵌入式](https://www.yuque.com/g/wenzhimo/qianrushi/collaborator/join?token=jJbeqT6BIV3fngcc&source=book_collaborator)中查看；也可以在[我的博客-嵌入式分类](https://www.wenzhimo.xyz/category/article/programing/%e5%b5%8c%e5%85%a5%e5%bc%8f/)中查看。

你可能会看到commit里有很多名为“同步”的提交，这是因为学生党使用两台不同的电脑进行开发，会将代码从本地仓库同步到远程仓库，以保持代码的最新版本。不使用远程桌面是因为校园网太差了。

## 使用 VS Code 开发 STM32

自2026.3.3起（即23FreeRTOS及后续内容，不包括24PID），本项目使用 Visual Studio Code 作为开发环境，且开发芯片使用 STM32F429IGT6，为 STM32 微控制器编程提供现代化的开发体验。

### 快速入门

如果您是第一次使用 VS Code 开发 STM32，我推荐参考以下教程：



[**【爽！手把手教你用 VSCode 开发 STM32【大人，时代变啦！！！】】**](https://www.bilibili.com/video/BV1QfbpzGENy/?share_source=copy_web&vd_source=a9d52604c74e8a682e8f5b4ef43d9198)

该视频详细展示了如何配置 VS Code 环境、安装必要的扩展和工具链，以及开始您的第一个 STM32 项目。

如何生成hex文件：
在build目录下，使用命令
`arm-none-eabi-objcopy -O ihex <工程名字>.elf <工程名字>.hex`
即可。

工程名字可以查看cmake输出，找到类似
`[build] [36/36] Linking C executable F429BASE.elf`
的语句即可，F429BASE就是工程名字。

### 如何添加第三方库、自己编写的代码？

#### 对于自己编写的代码
假设你要把 ThirdParty/MyLib/ 目录下的所有文件都加入编译，你可以将以下代码放入你的 CMakeLists.txt 中：

1. 扫描源文件并加入编译
使用 file(GLOB_RECURSE ...) 可以递归扫描目标文件夹及其所有子文件夹。

```CMake
# 使用 GLOB_RECURSE 递归扫描所有的 .c 文件，并带上 CONFIGURE_DEPENDS 防止文件更新不触发编译的问题
file(GLOB_RECURSE THIRDPARTY_SOURCES CONFIGURE_DEPENDS 
    "ThirdParty/MyLib/*.c"
    # 如果有汇编文件也可以一起加上
    # "ThirdParty/MyLib/*.s" 
)

# 将扫描到的源文件列表加入到工程的编译源中
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${THIRDPARTY_SOURCES}
)
```
(注：如果不需要扫描子文件夹，只需要当前文件夹，把 GLOB_RECURSE 换成 GLOB 即可。)

2. 处理头文件 (.h)
这里有一个关于头文件的常见误区：你不需要把每个 .h 文件都塞进 target_sources 里参与编译。编译器只需要知道“去哪个文件夹找头文件”即可。

你需要做的是把包含头文件的目录加进去：

```CMake
# 告诉编译器去哪里找头文件 (填的是目录路径，不是通配符)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ThirdParty/MyLib
    ThirdParty/MyLib/inc  # 如果它的头文件在单独的 inc 文件夹下
)
```
#### 对于自带cmake配置的库，如FreeRTOS、LVGL等
第 1 步：整理目录与配置文件

假设你把 LVGL 下载到了 ThirdParty/lvgl 目录下。
对于 LVGL 来说，它通常需要一个配置文件 lv_conf.h。你可以把这个文件放在 ThirdParty/ 根目录下，结构如下：

```Plaintext
你的工程目录/
├── Core/
├── ThirdParty/
│   ├── lv_conf.h        <-- LVGL 的配置文件 (从 lv_conf_template.h 复制改名而来)
│   └── lvgl/            <-- LVGL 源码文件夹 (里面有它自己的 CMakeLists.txt)
├── CMakeLists.txt       <-- CubeMX生成的主 CMake 文件
```
第 2 步：在主 CMakeLists.txt 中添加子目录

打开 STM32CubeMX 生成的主 CMakeLists.txt。

使用 add_subdirectory() 命令引入 LVGL：

```CMake
# 1. 告诉编译器去哪里找 lv_conf.h (这一步对 LVGL 很重要)
# 必须把包含 lv_conf.h 的目录加到编译器的搜索路径中
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/ThirdParty
)

# 2. 将 lvgl 文件夹作为一个整体（子 CMake 工程）添加进来
add_subdirectory(ThirdParty/lvgl)
```
原理：当 CMake 执行到 add_subdirectory 时，它会跑去执行 ThirdParty/lvgl/CMakeLists.txt，根据 LVGL 作者写好的规则去组织编译，并生成一个名为 lvgl 的库目标（Target）。

第 3 步：将 LVGL 链接到你的 STM32 工程

上一步只是让 CMake 知道了如何编译 LVGL，但还需要把你编译出来的 STM32 固件和 LVGL 库“绑定”在一起。

在主 CMakeLists.txt 中找到链接库的地方，使用 target_link_libraries：

```CMake
# 将 lvgl 库链接到你的主工程 ${CMAKE_PROJECT_NAME} 上
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE 
    lvgl
)
```
(注意：LVGL 在它的 CMakeLists 中定义的 Target 名字就是 lvgl。如果是其他库，你需要看它们文档里定义的库名叫什么)
