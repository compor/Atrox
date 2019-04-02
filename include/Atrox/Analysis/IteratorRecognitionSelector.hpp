//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include <vector>
// using std::vector

#include <memory>
// using std::unique_ptr

namespace llvm {
class Function;
class BasicBlock;
class LoopInfo;
class Loop;
class MemoryDependenceResults;
} // namespace llvm

namespace iteratorrecognition {
class IteratorRecognitionInfo;
} // namespace iteratorrecognition

namespace atrox {

class IteratorRecognitionSelector {
  llvm::Function *CurFunc;
  llvm::LoopInfo *CurLI;
  llvm::MemoryDependenceResults *CurMD;
  std::vector<llvm::BasicBlock *> Blocks;
  std::unique_ptr<iteratorrecognition::IteratorRecognitionInfo> Info;

  void calculate(llvm::Loop &L);

public:
  IteratorRecognitionSelector(llvm::Function &Func, llvm::LoopInfo &LI,
                              llvm::MemoryDependenceResults *MD);

  const std::vector<llvm::BasicBlock *> &getBlocks(llvm::Loop &L) {
    calculate(L);

    return Blocks;
  }
};

} // namespace atrox

