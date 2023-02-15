
# file(GLOB_RECURSE HEADERS  "../include/*.hpp" "../include/*.h")

set(
  SOURCES
  network/http/handler.cpp
  network/http/parser.cpp
  network/http/request.cpp
  network/http/request_parser.cpp
  network/http/server.cpp
  ini.cpp
  log.cpp
  main.cpp
  # debug.cpp
)


add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC ../include)
