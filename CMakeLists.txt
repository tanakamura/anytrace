cmake_minimum_required(VERSION 2.6)

set(ENABLE_WERROR CACHE BOOL OFF)

set(CMAKE_C_FLAGS "-fvisibility=hidden -Wall -Wmissing-prototypes -Werror=incompatible-pointer-types -fPIC -Werror")

if(ENABLE_WERROR)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
endif()


include(TestBigEndian)
include(CheckIncludeFile)

test_big_endian(ANYTRACE_BIG_ENDIAN)

include_directories(.)
include_directories(${CMAKE_BINARY_DIR})

check_include_file(dwarf.h HAVE_DWARF_H)
add_definitions(-DINSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}")

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
  npr/red-black-tree.c
)

add_library(atr SHARED
  anytrace/atr-errors.c
  anytrace/atr.c
  anytrace/atr-process.c
  anytrace/atr-file.c
  anytrace/atr-backtrace.c
  anytrace/atr-language-module.c
  )

add_executable(anytrace
  anytrace/anytrace.c)

target_link_libraries(atr npr dl)
target_link_libraries(anytrace atr)

set_property(TARGET anytrace PROPERTY C_STANDARD 99)
set_property(TARGET atr PROPERTY C_STANDARD 99)
set_target_properties(atr PROPERTIES VERSION "0.0.1" SOVERSION "0")

add_subdirectory(npr/test)

configure_file (anytrace/config.h.in config.h)

add_subdirectory(languages/python)

install(TARGETS atr anytrace
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib)

install(FILES
  anytrace/atr.h
  anytrace/atr-file.h
  anytrace/atr-errors.h
  anytrace/atr-language-module.h
  DESTINATION include/anytrace)
