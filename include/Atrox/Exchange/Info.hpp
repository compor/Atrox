//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "Atrox/Support/IR/ArgSpec.hpp"

#include "llvm/Analysis/LoopInfo.h"
// using llvm::Loop

#include <vector>
// using std::vector

namespace llvm {
class Function;
} // namespace llvm

namespace atrox {

struct FunctionArgSpec {
  llvm::Function *Func;
  llvm::Loop *CurLoop;
  std::vector<ArgSpec> Args;
};

} // namespace atrox

