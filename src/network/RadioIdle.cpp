#include "RadioIdle.h"

#ifndef SIMULATOR
#include <HalPowerManager.h>
#endif

namespace radioidle {

bool enabled() {
#ifdef SIMULATOR
  return false;
#else
  return HalPowerManager::radioOffWhenIdle();
#endif
}

bool release(const char* tag) {
#ifdef SIMULATOR
  (void)tag;
  return false;
#else
  return HalPowerManager::releaseIdleRadio(tag);
#endif
}

}  // namespace radioidle
