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

#include "llvm/ADT/Optional.h"
// using llvm::Optional

#include <map>
// using std::map

#include <cassert>
// using cassert

namespace llvm {
class ScalarEvolution;
class SCEV;
class raw_ostream;
} // namespace llvm

namespace atrox {

struct LoopIterationSpaceInfo {
  llvm::PHINode *InductionVariable = nullptr;
  llvm::SCEV *Start = nullptr;
  llvm::SCEV *End = nullptr;
  unsigned TripCount = 0u;
};

class LoopBoundsAnalyzer {
  llvm::Loop *TopL;
  llvm::LoopInfo *LI;
  llvm::ScalarEvolution *SE;
  std::map<llvm::Loop *, LoopIterationSpaceInfo> LoopBoundsMap;

  void reset() {
    TopL = nullptr;
    LoopBoundsMap.clear();
  }

public:
  LoopBoundsAnalyzer() = delete;

  LoopBoundsAnalyzer(llvm::LoopInfo &CurLI, llvm::ScalarEvolution &CurSE)
      : TopL(nullptr), LI(&CurLI), SE(&CurSE) {}

  bool analyze(llvm::Loop *CurL);

  void evaluate();

  bool isValueUsedInLoopNestConditions(
      llvm::Value *V, llvm::Loop *L,
      llvm::SmallPtrSetImpl<llvm::Instruction *> *Conditions = nullptr);

  bool isValueUsedOnlyInLoopNestConditions(
      llvm::Value *V, llvm::Loop *L,
      const llvm::SmallPtrSetImpl<llvm::BasicBlock *> &Interesting);

  bool isValueOuterLoopInductionVariable(llvm::Value *V, llvm::Loop *L);

  bool isValueInnerLoopInductionVariable(llvm::Value *V, llvm::Loop *L);

  llvm::Optional<LoopIterationSpaceInfo> getInfo(llvm::Loop *L) const;

  llvm::Optional<LoopIterationSpaceInfo> getInfo(llvm::Value *IndVar) const;

  void print(llvm::raw_ostream &OS) const;
};

} // namespace atrox

