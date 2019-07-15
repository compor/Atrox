//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "Atrox/Support/IR/ArgDirection.hpp"

#include <string>
// using std::string

namespace atrox {

struct ArgSpec {
  std::string Name;
  ArgDirection Direction;
  bool IteratorDependent;
};

} // namespace atrox

