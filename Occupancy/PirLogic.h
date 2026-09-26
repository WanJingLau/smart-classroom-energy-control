#ifndef PIR_LOGIC_H
#define PIR_LOGIC_H

#include <stdint.h>

inline uint8_t encodePirSignal(bool motionDetected) {
  return motionDetected ? 1 : 0;
}

#endif
