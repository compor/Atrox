//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "Atrox/Support/IR/ArgSpec.hpp"

#include <vector>
// using std::vector

namespace llvm {
class Function;
} // namespace llvm

namespace atrox {

struct FunctionArgSpec {
  llvm::Function *Func;
  std::vector<ArgSpec> Args;
};

} // namespace atrox

