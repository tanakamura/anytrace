
add_library(atr-python SHARED
  atr-python.c)

install(TARGETS atr-python
  LIBRARY DESTINATION lib/anytrace)
