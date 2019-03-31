//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include <vector>
// using std::vector

namespace llvm {
class Function;
class BasicBlock;
class LoopInfo;
class Loop;
} // namespace llvm

namespace atrox {

class IteratorRecognitionSelector {
  llvm::Function *CurFunc;
  llvm::LoopInfo *CurLI;
  std::vector<llvm::BasicBlock *> Blocks;

  void calculate(llvm::Loop &L);

public:
  IteratorRecognitionSelector(llvm::Function &Func, llvm::LoopInfo &LI);

  const std::vector<llvm::BasicBlock *> &getBlocks(llvm::Loop &L) {
    calculate(L);

    return Blocks;
  }
};

} // namespace atrox

