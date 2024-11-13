# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.20)

set("EXTRA_COMPONENT_DIRS" 
    "/Users/larshostruppedersen/motorskills/HockeyBar/Esp32/Components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hockeybar_firmware)

# message(STATUS ">>>>> HBF_BEFORE_IF " $ENV{HBF_WIFI_SSID} )
# if(DEFINED ENV{HBF_WIFI_SSID}) 
#     message(STATUS ">>>>> HBF_INSIDE_IF")
#     # define preprocessor macro with the same name

#     # add_compile_definitions(HBF_WIFI_SSID=$ENV{HBF_WIFI_SSID} )
#     add_compile_options(-DHBF_WIFI_SSID=$ENV{HBF_WIFI_SSID})
# endif()
# message(STATUS ">>>>> HBF_AFTER_IF")

