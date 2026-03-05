
# STM32 学习项目展示
## 学习笔记查看
对于`21KaoQingSystem_Server`及以前的的项目，学习笔记包含在对应外设代码文件中。

对于`22STM32G4_GPIO_MOTOR`及以后的项目，学习笔记可在[语雀知识库-嵌入式](https://www.yuque.com/g/wenzhimo/qianrushi/collaborator/join?token=jJbeqT6BIV3fngcc&source=book_collaborator)中查看；也可以在[我的博客-嵌入式分类](https://www.wenzhimo.xyz/category/article/programing/%e5%b5%8c%e5%85%a5%e5%bc%8f/)中查看。


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