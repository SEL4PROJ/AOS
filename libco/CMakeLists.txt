add_library(libco STATIC libco.c aarch64.c arm.c settings.h libco.h)
target_include_directories(libco PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(libco muslc)