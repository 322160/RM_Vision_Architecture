cmake_minimum_required(VERSION 3.10)
project(RM_Vision_Architecture)

set(CMAKE_CXX_STANDARD 14)
find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)
#find_package:通知编译器，找OpenCV和Threads这两个仓库
include_directories(include)
include_directories(${OpenCV_INCLUDE_DIRS})   # 显式带上 OpenCV 头文件路径，换机器也能编

#add_executable:核心指令，添加可执行文件，第一个参数是可执行文件名，第二个参数是源文件列表

# ==========================================
# 程序 1：主视觉程序（OpenCV 传统视觉多颜色检测）
# ==========================================
add_executable(vision_app
    src/main.cpp
    src/TrafficLightDetector.cpp
)
target_link_libraries(vision_app ${OpenCV_LIBS} Threads::Threads)


# ==========================================
# 程序 2：PnP 纯数学解算测试（纯图片无交互）
#add_executable(pnp_app src/pnp_test.cpp)
#target_link_libraries(pnp_app ${OpenCV_LIBS})

# 程序 3：鼠标点击版 PnP 测试（带图形界面）
#add_executable(rm_pnp_mouse_app src/rm_pnp_mouse.cpp)
#target_link_libraries(rm_pnp_mouse_app ${OpenCV_LIBS})

# 程序 4：串口打包测试（纯 C++ 不需要 OpenCV）
#add_executable(serial_app src/serial_test.cpp)
#target_link_libraries(serial_app ${OpenCV_LIBS})
