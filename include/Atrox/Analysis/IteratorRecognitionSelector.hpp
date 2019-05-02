//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "IteratorRecognition/Analysis/IteratorRecognition.hpp"

#include "llvm/ADT/SmallVector.h"
// using llvm::SmallVector

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
  llvm::LoopInfo *CurLI;
  iteratorrecognition::IteratorRecognitionInfo &Info;

  void calculate(llvm::Loop &L,
                 llvm::SmallVectorImpl<llvm::BasicBlock *> &Blocks);

public:
  explicit IteratorRecognitionSelector(
      iteratorrecognition::IteratorRecognitionInfo &ITRInfo);

  void getBlocks(llvm::Loop &L,
                 llvm::SmallVectorImpl<llvm::BasicBlock *> &Blocks) {
    calculate(L, Blocks);
  }
};

} // namespace atrox

