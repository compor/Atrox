//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "llvm/ADT/SmallVector.h"
// usiing llvm::SmallVector

#include <cassert>
// using assert

namespace llvm {
class BasicBlock;
class Loop;
} // namespace llvm

namespace atrox {

class NaiveSelector {
  void calculate(llvm::Loop &L,
                 llvm::SmallVectorImpl<llvm::BasicBlock *> &Blocks);

public:
  explicit NaiveSelector() = default;

  void getBlocks(llvm::Loop &L,
                 llvm::SmallVectorImpl<llvm::BasicBlock *> &Blocks) {
    calculate(L, Blocks);
  }
};

} // namespace atrox

