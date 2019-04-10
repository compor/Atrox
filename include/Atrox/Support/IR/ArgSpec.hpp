//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "Atrox/Support/IR/ArgDirection.hpp"

namespace atrox {

struct ArgSpec {
  ArgDirection Direction;
  bool IteratorDependent;
};

} // namespace atrox

