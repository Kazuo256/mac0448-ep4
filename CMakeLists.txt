
cmake_minimum_required (VERSION 2.8)

project (ep4 CXX C)
include_directories (src)

set (
  EP4_SRC
  src/main.cxx
  src/ep4.cxx
  src/ep4.h
  src/network.cxx
  src/network.h
  src/packet.cxx
  src/packet.h
  src/transmissionqueue.cxx
  src/transmissionqueue.h
  src/router.cxx
  src/router.h
  src/routerlogic.h
  src/sharedlogic.h
  src/sharedlogic.cxx
)

add_executable (ep4 ${EP4_SRC})

set_target_properties (
  ep4
  PROPERTIES COMPILE_FLAGS
  "-ansi -Wall -g -O0"
)

