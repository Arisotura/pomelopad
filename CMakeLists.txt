cmake_minimum_required(VERSION 3.27)
project(pomelopad)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

add_executable(pomelopad
        src/main.cpp
        src/ARMInterpreter.h
        src/ARM_InstrTable.h
        src/ARMInterpreter_Branch.h
        src/ARM.cpp
        src/ARMInterpreter_LoadStore.cpp
        src/ARMInterpreter_ALU.cpp
        src/ARMInterpreter_ALU.h
        src/CP15.cpp
        src/ARM.h
        src/ARMInterpreter.cpp
        src/ARMInterpreter_LoadStore.h
        src/ARMInterpreter_Branch.cpp
        src/Platform.cpp
        src/Platform.h
        src/SPI.cpp
        src/WUP.cpp
        src/SPI.cpp
        src/UIC_HLE.cpp
        src/Flash.cpp
        src/DMA.cpp
        src/UART.cpp
        src/I2C.cpp
        src/LCD.cpp
        src/AudioAmp.cpp
        src/Camera.cpp
        src/Video.cpp
        src/SDIO.cpp
        src/Wifi.cpp
        src/Audio.cpp
)

target_link_libraries(pomelopad ${SDL2_LIBRARIES})
