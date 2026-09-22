cmake_minimum_required (VERSION 3.17)

project ("winutil" C CXX RC)

add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN)

add_executable (winutil WIN32 
    "src/icon.rc" 
    "src/pch.h"
    "src/switcher.cpp" 
    "src/main.cpp" 
)

set_property(TARGET winutil PROPERTY CXX_STANDARD 20)
target_precompile_headers(winutil PRIVATE "src/pch.h")
target_link_libraries(winutil PRIVATE dwmapi)
