//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "IteratorRecognition/Analysis/IteratorRecognition.hpp"

#include "llvm/ADT/SmallVector.h"
// using llvm::SmallVector

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
  std::unique_ptr<iteratorrecognition::IteratorRecognitionInfo> Info;

  void calculate(llvm::Loop &L,
                 llvm::SmallVectorImpl<llvm::BasicBlock *> &Blocks);

public:
  IteratorRecognitionSelector(llvm::Function &Func, llvm::LoopInfo &LI,
                              llvm::MemoryDependenceResults *MD);

  void getBlocks(llvm::Loop &L,
                 llvm::SmallVectorImpl<llvm::BasicBlock *> &Blocks) {
    calculate(L, Blocks);
  }
};

} // namespace atrox

