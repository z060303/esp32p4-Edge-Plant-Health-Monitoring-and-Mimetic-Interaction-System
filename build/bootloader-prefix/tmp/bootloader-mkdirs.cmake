# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/Espressif/frameworks/esp-idf-v5.4.4/components/bootloader/subproject"
  "C:/Users/ZHENG/Desktop/vscodeproject/plant_detect/esp32p4-Edge-Plant-Health-Monitoring-and-Mimetic-Interaction-System-plant_ai_01/camera_mipi_headless/build/bootloader"
  "C:/Users/ZHENG/Desktop/vscodeproject/plant_detect/esp32p4-Edge-Plant-Health-Monitoring-and-Mimetic-Interaction-System-plant_ai_01/camera_mipi_headless/build/bootloader-prefix"
  "C:/Users/ZHENG/Desktop/vscodeproject/plant_detect/esp32p4-Edge-Plant-Health-Monitoring-and-Mimetic-Interaction-System-plant_ai_01/camera_mipi_headless/build/bootloader-prefix/tmp"
  "C:/Users/ZHENG/Desktop/vscodeproject/plant_detect/esp32p4-Edge-Plant-Health-Monitoring-and-Mimetic-Interaction-System-plant_ai_01/camera_mipi_headless/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/ZHENG/Desktop/vscodeproject/plant_detect/esp32p4-Edge-Plant-Health-Monitoring-and-Mimetic-Interaction-System-plant_ai_01/camera_mipi_headless/build/bootloader-prefix/src"
  "C:/Users/ZHENG/Desktop/vscodeproject/plant_detect/esp32p4-Edge-Plant-Health-Monitoring-and-Mimetic-Interaction-System-plant_ai_01/camera_mipi_headless/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/ZHENG/Desktop/vscodeproject/plant_detect/esp32p4-Edge-Plant-Health-Monitoring-and-Mimetic-Interaction-System-plant_ai_01/camera_mipi_headless/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/ZHENG/Desktop/vscodeproject/plant_detect/esp32p4-Edge-Plant-Health-Monitoring-and-Mimetic-Interaction-System-plant_ai_01/camera_mipi_headless/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
