cmake_minimum_required(VERSION 2.6)

set(ENABLE_WERROR CACHE BOOL OFF)

set(CMAKE_C_FLAGS "-Wall -Wmissing-prototypes -Werror=incompatible-pointer-types")

if(ENABLE_WERROR)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
endif()


include(TestBigEndian)
include(CheckIncludeFile)

test_big_endian(ANYTRACE_BIG_ENDIAN)

include_directories(.)
include_directories(${CMAKE_BINARY_DIR})

check_include_file(dwarf.h HAVE_DWARF_H)

if (NOT HAVE_DWARF_H)
  message("please install libdwarf-dev (dwarf.h not found)")
endif()

add_library(npr
  npr/int-map.c
  npr/strbuf.c
  npr/mempool-c.c
  npr/chunk-alloc.c
  npr/varray.c
  npr/symbol.c
)

add_library(atr
  anytrace/atr-errors.c
  anytrace/atr.c
  anytrace/atr-process.c
  anytrace/atr-file.c
  anytrace/atr-backtrace.c
  )

add_executable(anytrace
  anytrace/anytrace.c)

target_link_libraries(atr npr)
target_link_libraries(anytrace atr)

set_property(TARGET anytrace PROPERTY C_STANDARD 99)
set_property(TARGET atr PROPERTY C_STANDARD 99)



configure_file (anytrace/config.h.in config.h)
