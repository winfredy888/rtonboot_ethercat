# vim: syntax=cmake
cmake_minimum_required(VERSION 2.8.11)

set(LIBRTSDK_SRC rtsdk.c)
add_library(rtsdk SHARED ${LIBRTSDK_SRC})
add_library(rtsdk_static STATIC ${LIBRTSDK_SRC})
set_target_properties(rtsdk_static PROPERTIES OUTPUT_NAME "rtsdk")
INSTALL(TARGETS rtsdk rtsdk_static LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
INSTALL(FILES rtsdk.h DESTINATION include)


