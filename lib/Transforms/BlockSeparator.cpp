//
//
//

#include "Atrox/Transforms/BlockSeparator.hpp"

#include "IteratorRecognition/Analysis/IteratorRecognition.hpp"

#include "llvm/IR/BasicBlock.h"
// using llvm::BasicBlock

#include "llvm/IR/Instructions.h"
// using llvm::BranchInst

#include "llvm/IR/Dominators.h"
// using llvm::DominatorTree

#include "llvm/Analysis/LoopInfo.h"
// using llvm::Loop
// using llvm::LoopInfo

#include "llvm/Transforms/Utils/BasicBlockUtils.h"
// using llvm::SplitBlock

#include "llvm/Support/Debug.h"
// using LLVM_DEBUG macro
// using llvm::dbgs

#include <algorithm>
// using std::reverse

#include <iterator>
// using std::prev

#define DEBUG_TYPE "atrox-separator"

namespace atrox {

Mode GetMode(const llvm::Instruction &Inst, const llvm::Loop &CurLoop,
             const iteratorrecognition::IteratorInfo &Info) {
  return Info.isIterator(&Inst) ? Mode::Iterator : Mode::Payload;
}

bool FindPartitionPoints(const llvm::Loop &CurLoop,
                         const iteratorrecognition::IteratorInfo &Info,
                         BlockModeMapTy &Modes,
                         BlockModeChangePointMapTy &Points) {
  for (auto bi = CurLoop.block_begin(), be = CurLoop.block_end(); bi != be;
       ++bi) {
    auto *bb = *bi;
    // auto firstI = bb->getFirstInsertionPt();
    // auto lastSeenMode = InvertMode(GetMode(*firstI, CurLoop, Info));
    bool hasAllSameModeInstructions = true;

    auto phisMode = GetMode(*bb->begin(), CurLoop, Info);
    for (auto &e : bb->phis()) {
      auto mode = GetMode(e, CurLoop, Info);
      if (phisMode != mode) {
        hasAllSameModeInstructions = false;
        break;
      }

      phisMode = mode;
    }

    // these are just for auto type deduction
    auto firstI = bb->getFirstInsertionPt();
    auto lastSeenMode = GetMode(*firstI, CurLoop, Info);

    if (hasAllSameModeInstructions) {
      firstI = bb->begin();
      lastSeenMode = GetMode(*firstI, CurLoop, Info);
    } else {
      firstI = bb->getFirstInsertionPt();
      lastSeenMode = GetMode(*firstI, CurLoop, Info);

      auto modeChangePt = std::make_pair(&*firstI, InvertMode(lastSeenMode));
      if (Points.find(bb) == Points.end())
        Points.emplace(bb, std::vector<BlockModeChangePointTy>{});

      Points.at(bb).push_back(modeChangePt);
    }

    hasAllSameModeInstructions = true;

    for (auto ii = firstI, ie = bb->end(); ii != ie; ++ii) {
      auto &inst = *ii;
      auto *br = llvm::dyn_cast<llvm::BranchInst>(&inst);
      bool isUncondBr = br && br->isUnconditional();
      auto curMode = GetMode(inst, CurLoop, Info);

      if (lastSeenMode != curMode && !isUncondBr) {
        hasAllSameModeInstructions = false;
        auto modeChangePt = std::make_pair(&inst, curMode);

        if (Points.find(bb) == Points.end())
          Points.emplace(bb, std::vector<BlockModeChangePointTy>{});

        Points.at(bb).push_back(modeChangePt);
        lastSeenMode = curMode;
      }
    }

    if (hasAllSameModeInstructions)
      Modes.emplace(bb, lastSeenMode);
  }

  return !Points.empty();
}

void SplitAtPartitionPoints(BlockModeChangePointMapTy &Points,
                            BlockModeMapTy &Modes, llvm::DominatorTree *DT,
                            llvm::LoopInfo *LI) {
  for (auto &e : Points) {
    auto *oldBB = e.first;
    Mode lastMode;
    std::reverse(e.second.begin(), e.second.end());

    for (auto &k : e.second) {
      auto *splitI = k.first;
      lastMode = k.second;
      Modes.emplace(llvm::SplitBlock(oldBB, splitI, DT, LI), lastMode);
    }

    Modes.emplace(oldBB, InvertMode(lastMode));
  }

  return;
}

} // namespace atrox

