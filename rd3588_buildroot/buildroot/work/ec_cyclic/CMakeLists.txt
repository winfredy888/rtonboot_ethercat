# vim: syntax=cmake
cmake_minimum_required(VERSION 2.8.11)

set(EC_CYCLIC_SRC ec_cyclic.c)

set(LIBRTSDK_DIR ".")

add_library(rtsdk STATIC IMPORTED)

#find_package(PkgConfig)
#pkg_check_modules(RT rt)

set_property(TARGET rtsdk PROPERTY IMPORTED_LOCATION ${LIBRTSDK_DIR}/librtsdk.a)

add_executable(ec_cyclic ${EC_CYCLIC_SRC})

target_link_libraries(ec_cyclic rtsdk pthread rt)
 


