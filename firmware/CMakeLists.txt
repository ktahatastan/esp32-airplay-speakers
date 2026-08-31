# Harman Kardom firmware, ESP32-S3 N16R8 (ADR-0010).
#
#   . $IDF_PATH/export.sh
#   idf.py -C firmware build
#
# PROJECT_VER is taken from firmware/version.txt by ESP-IDF. It must stay strict
# SemVer: the OTA client compares it numerically, and the release pipeline checks
# it against the Git tag (ADR-0008).
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(harman-kardom)
