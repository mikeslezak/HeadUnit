cmake_minimum_required(VERSION 3.21)
project(eglfs-test LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Quick)

qt_add_executable(eglfs-test main.cpp)
qt_add_qml_module(eglfs-test
    URI EglfsTest
    VERSION 1.0
    QML_FILES main.qml
)
target_link_libraries(eglfs-test PRIVATE Qt6::Quick)
