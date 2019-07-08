//
//
//

#pragma once

#include "llvm/Analysis/LoopInfo.h"
// using llvm::LoopInfo
// using llvm::Loop

#include "llvm/ADT/SmallPtrSet.h"
// using llvm::SmallPtrSetImpl
// using llvm::SmallPtrSet

#include <map>
// using std::map

#include <cassert>
// using cassert

namespace llvm {
class ScalarEvolution;
class SCEV;
} // namespace llvm

namespace atrox {

struct LoopIterationSpaceInfo {
  llvm::PHINode *InductionVariable = nullptr;
  llvm::SCEV *Start = nullptr;
  llvm::SCEV *End = nullptr;
  unsigned TripCount = 0u;
};

class LoopBoundsAnalyzer {
  const llvm::Loop *L;
  llvm::LoopInfo *LI;
  llvm::ScalarEvolution *SE;
  std::map<llvm::Loop *, LoopIterationSpaceInfo> LoopBoundsMap;

public:
  LoopBoundsAnalyzer() = delete;

  LoopBoundsAnalyzer(const llvm::Loop &CurL, llvm::LoopInfo &CurLI,
                     llvm::ScalarEvolution &CurSE)
      : L(&CurL), LI(&CurLI), SE(&CurSE) {
    assert(LI->getLoopFor(L->getHeader()) == L &&
           "Loop does not belong to this loop info object!");
    assert(L->getLoopDepth() == 1 && "Top level loop is required!");
  }

  bool isValueUsedInLoopNestConditions(
      llvm::Value *V, llvm::Loop *L,
      llvm::SmallPtrSetImpl<llvm::Instruction *> *Conditions = nullptr);

  bool isValueUsedOnlyInLoopNestConditions(
      llvm::Value *V, llvm::Loop *L,
      const llvm::SmallPtrSetImpl<llvm::BasicBlock *> &Interesting);

  bool analyze();
};

} // namespace atrox

