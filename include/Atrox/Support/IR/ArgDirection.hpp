//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include <cstdint>
// using uint8_t

namespace atrox {

enum ArgDirection : uint8_t {
  AD_Inbound = 1,
  AD_Outbound = 2,
  AD_Both = AD_Inbound | AD_Outbound,
};

inline int toInt(const ArgDirection AD) { return static_cast<int>(AD); }

inline bool isInbound(const ArgDirection AD) {
  return (toInt(AD) & toInt(AD_Inbound));
}

inline bool isOutbound(const ArgDirection AD) {
  return (toInt(AD) & toInt(AD_Outbound));
}

inline bool isOnlyInbound(const ArgDirection AD) {
  return (toInt(AD) & toInt(AD_Inbound)) && !(toInt(AD) & toInt(AD_Outbound));
}

inline bool isOnlyOutbound(const ArgDirection AD) {
  return (toInt(AD) & toInt(AD_Outbound)) && !(toInt(AD) & toInt(AD_Inbound));
}

} // namespace atrox

