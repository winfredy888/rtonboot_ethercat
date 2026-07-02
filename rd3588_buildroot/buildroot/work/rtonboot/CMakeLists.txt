# vim: syntax=cmake
cmake_minimum_required(VERSION 2.8.11)

set(RTONBOOT_SRC rtonboot.c)

set(LIBRTSDK_DIR ".")

add_library(rtsdk STATIC IMPORTED)

#find_package(PkgConfig)
#pkg_check_modules(RT rt)

set_property(TARGET rtsdk PROPERTY IMPORTED_LOCATION ${LIBRTSDK_DIR}/librtsdk.a)

add_executable(rtonboot ${RTONBOOT_SRC})

target_link_libraries(rtonboot rtsdk pthread rt)
 


