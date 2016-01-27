cmake_minimum_required(VERSION 2.6)

set(ENABLE_WERROR CACHE BOOL OFF)

set(CMAKE_C_FLAGS "-Wall -Wmissing-prototypes")

if(ENABLE_WERROR)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
endif()

include_directories(.)
add_library(npr
  npr/int-map.c
  npr/strbuf.c
  npr/mempool-c.c
  npr/chunk-alloc.c
)

add_library(atr
  anytrace/atr-errors.c
  anytrace/atr.c
  anytrace/atr-process.c
  )


add_executable(anytrace
  anytrace/anytrace.c)

target_link_libraries(atr npr)
target_link_libraries(anytrace atr)

set_property(TARGET anytrace PROPERTY C_STANDARD 99)
set_property(TARGET atr PROPERTY C_STANDARD 99)

