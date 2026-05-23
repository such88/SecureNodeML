# firmware/esp32/CMakeLists.txt
# Top-level CMakeLists for ESP-IDF project
#
# Build commands (from firmware/esp32/ directory):
#   idf.py set-target esp32
#   idf.py build
#   idf.py -p /dev/ttyUSB0 flash monitor

cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_gateway)
