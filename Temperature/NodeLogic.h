#ifndef NODE_LOGIC_H
#define NODE_LOGIC_H

#include <stdint.h>

struct NodeState {
  bool active;
  bool redLed;
  bool greenLed;
};

const uint8_t NODE_COMMAND_SLEEP = 0;
const uint8_t NODE_COMMAND_COMFORTABLE = 1;
const uint8_t NODE_COMMAND_HEATING = 2;
const uint8_t NODE_COMMAND_COOLING = 3;

inline NodeState nodeStateForCommand(uint8_t command,
                                     bool communicationTimedOut) {
  if (communicationTimedOut || command == NODE_COMMAND_SLEEP) {
    return {false, false, false};
  }

  if (command == NODE_COMMAND_HEATING) {
    return {true, true, false};
  }

  if (command == NODE_COMMAND_COOLING) {
    return {true, false, true};
  }

  if (command == NODE_COMMAND_COMFORTABLE) {
    return {true, false, false};
  }

  return {false, false, false};
}

#endif
