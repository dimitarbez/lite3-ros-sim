#include <cstdlib>
#include <iostream>
#include <string>

#include "sender.h"

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: probe_motion_sdk_control PORT sdk|robot|init\n";
    return 2;
  }
  const int port = std::atoi(argv[1]);
  if (port < 1024 || port > 65535) {
    std::cerr << "invalid loopback port\n";
    return 2;
  }
  Sender sender("127.0.0.1", static_cast<uint16_t>(port));
  const std::string operation(argv[2]);
  if (operation == "sdk") {
    sender.ControlGet(SDK);
  } else if (operation == "robot") {
    sender.ControlGet(ROBOT);
  } else if (operation == "init") {
    sender.RobotStateInit();
  } else {
    std::cerr << "unsupported operation\n";
    return 2;
  }
  return 0;
}
