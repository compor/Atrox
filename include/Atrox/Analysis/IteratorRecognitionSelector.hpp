//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "llvm/ADT/SmallVector.h"
// usiing llvm::SmallVector

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
  llvm::SmallVector<llvm::BasicBlock *, 32> Blocks;
  std::unique_ptr<iteratorrecognition::IteratorRecognitionInfo> Info;

  void calculate(llvm::Loop &L);

public:
  IteratorRecognitionSelector(llvm::Function &Func, llvm::LoopInfo &LI,
                              llvm::MemoryDependenceResults *MD);

  const llvm::SmallVectorImpl<llvm::BasicBlock *> &getBlocks(llvm::Loop &L) {
    Blocks.clear();
    calculate(L);

    return Blocks;
  }
};

} // namespace atrox

