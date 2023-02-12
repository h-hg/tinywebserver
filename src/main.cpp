// todo
#include <fstream>
#include <iostream>
#include <streambuf>
#include <string>
#include <string_view>

#include "tinywebserver/ini.h"
#include "tinywebserver/network/http/server.h"

INI read_config(const std::string &filename) {
  std::fstream fs(filename);
  std::string str((std::istreambuf_iterator<char>(fs)),
                  std::istreambuf_iterator<char>());
  std::string_view sv(str.begin(), str.end());
  auto ini = INI::parse(sv);
}

int main() {
  INI ini;
  try {
    ini = read_config("./config.ini");
  } catch (...) {
    std::cerr << "Can't parser config file." << std::endl;
    return -1;
  }
  http::Server server;
  // set server
  uint16_t port = std::stoi(ini.get("server", "port", "8888"));
  server.listen(port, ini.get("server", "adress"));
  // todo
  server.start();
  return 0;
}
