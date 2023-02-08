set(
  SOURCES
  network/http/parser.cpp
  network/http/request.cpp
  ini.cpp
  log.cpp
  main.cpp
  timer.cpp
)


add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC ../include)
