//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include <vector>
// using std::vector

#include <cassert>
// using assert

namespace llvm {
class BasicBlock;
class Loop;
} // namespace llvm

namespace atrox {

class NaiveSelector {
  llvm::Loop *CurLoop;
  std::vector<llvm::BasicBlock *> Blocks;
  mutable bool HasChanged;

  void calculate();

public:
  explicit NaiveSelector(llvm::Loop *L) : CurLoop(L), HasChanged(true) {}

  void setLoop(llvm::Loop *L) {
    CurLoop = L;
    HasChanged = true;
  }

  const std::vector<llvm::BasicBlock *> &getBlocks() {
    assert(CurLoop && "Loop pointer is empty!");

    if (HasChanged) {
      calculate();
      HasChanged = false;
    }

    return Blocks;
  }
};

} // namespace atrox

