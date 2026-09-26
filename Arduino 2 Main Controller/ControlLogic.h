#ifndef CONTROL_LOGIC_H
#define CONTROL_LOGIC_H

#include <stdint.h>

struct MotionState {
  bool motionSeen;
  uint32_t lastMotionTime;
};

const uint8_t COMMAND_SLEEP = 0;
const uint8_t COMMAND_COMFORTABLE = 1;
const uint8_t COMMAND_HEATING = 2;
const uint8_t COMMAND_COOLING = 3;

const uint16_t HEATING_THRESHOLD_CENTI_C = 1500;
const uint16_t COOLING_THRESHOLD_CENTI_C = 2500;

inline bool decodePirSignal(uint8_t value, bool& motion) {
  if (value > 1) {
    return false;
  }

  motion = value == 1;
  return true;
}

inline MotionState recordMotionSample(MotionState state, bool motion,
                                      uint32_t now) {
  if (motion) {
    state.motionSeen = true;
    state.lastMotionTime = now;
  }

  return state;
}

inline bool occupancyIsActive(const MotionState& state, uint32_t now,
                              uint32_t timeout) {
  return state.motionSeen &&
         static_cast<uint32_t>(now - state.lastMotionTime) <= timeout;
}

inline uint8_t determineCommand(bool occupied, uint16_t temperatureCentiC) {
  if (!occupied) {
    return COMMAND_SLEEP;
  }

  if (temperatureCentiC <= HEATING_THRESHOLD_CENTI_C) {
    return COMMAND_HEATING;
  }

  if (temperatureCentiC >= COOLING_THRESHOLD_CENTI_C) {
    return COMMAND_COOLING;
  }

  return COMMAND_COMFORTABLE;
}

#endif
