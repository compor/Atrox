//
//
//

#include "Atrox/Analysis/IteratorRecognitionSelector.hpp"

#include "private/PDGUtils.hpp"

#include "private/ITRUtils.hpp"

#include "llvm/Analysis/LoopInfo.h"
// using llvm::LoopInfo
// using llvm::Loop

#include "llvm/Analysis/MemoryDependenceAnalysis.h"
// using llvm::MemoryDependenceResults

#include "llvm/Analysis/LoopIterator.h"
// using llvm::LoopBlocksRPO

#include "llvm/IR/Function.h"
// using llvm::Function

#include "llvm/ADT/SetVector.h"
// usiing llvm::SmallSetVector

#include "llvm/Support/Debug.h"
// using LLVM_DEBUG macro
// using llvm::dbgs

#include <algorithm>
// using std::find

#define DEBUG_TYPE "atrox-selector"

namespace atrox {

IteratorRecognitionSelector::IteratorRecognitionSelector(
    llvm::Function &Func, llvm::LoopInfo &LI, llvm::MemoryDependenceResults *MD)
    : CurLI(&LI) {
  auto pdgraph = BuildPDG(Func, MD);
  Info = BuildITRInfo(LI, *pdgraph);
}

IteratorRecognitionSelector::IteratorRecognitionSelector(
    std::unique_ptr<iteratorrecognition::IteratorRecognitionInfo> ITRInfo)
    : CurLI(const_cast<llvm::LoopInfo *>(&ITRInfo->getLoopInfo())),
      Info(std::move(ITRInfo)) {}

void IteratorRecognitionSelector::calculate(
    llvm::Loop &L, llvm::SmallVectorImpl<llvm::BasicBlock *> &Blocks) {
  llvm::LoopBlocksRPO RPOTraversal(&L);
  RPOTraversal.perform(CurLI);
  llvm::SmallVector<llvm::BasicBlock *, 16> blocks{RPOTraversal.begin(),
                                                   RPOTraversal.end()};
  llvm::SmallSetVector<llvm::BasicBlock *, 16> selected;

  auto infoOrError = Info->getIteratorInfoFor(&L);

  if (!infoOrError) {
    LLVM_DEBUG(llvm::dbgs()
                   << "No iterator information available for loop with header: "
                   << *L.getHeader()->getTerminator() << '\n';);
    return;
  }
  auto &info = *infoOrError;
  llvm::SmallVector<llvm::BasicBlock *, 8> payloadBlocks;

  iteratorrecognition::GetPayloadOnlyBlocks(info, L, payloadBlocks);

  blocks.erase(std::remove(blocks.begin(), blocks.end(), L.getHeader()),
               blocks.end());

  llvm::SmallVector<llvm::BasicBlock *, 8> latches;
  L.getLoopLatches(latches);
  for (auto *b : latches) {
    blocks.erase(std::remove(blocks.begin(), blocks.end(), b), blocks.end());
  }

  for (auto *bb : blocks) {
    if (selected.count(bb)) {
      continue;
    }

    if (bb == L.getHeader()) {
      selected.insert(bb);
    } else if (CurLI->isLoopHeader(bb)) {
      auto *innerLoop = CurLI->getLoopFor(bb);

      if (!iteratorrecognition::HasPayloadOnlyBlocks(info, *innerLoop)) {
        LLVM_DEBUG(llvm::dbgs()
                       << "Mixed blocks in inner loop with header: "
                       << *innerLoop->getHeader()->getTerminator() << '\n';);
        break;
      }

      // and this inner loop is all payload

      llvm::LoopBlocksRPO innerRPOTraversal(innerLoop);
      innerRPOTraversal.perform(CurLI);

      for (auto *e : innerRPOTraversal) {
        selected.insert(e);
      }
    } else {
      auto found = std::find(payloadBlocks.begin(), payloadBlocks.end(), bb);
      if (found == payloadBlocks.end()) {
        LLVM_DEBUG(llvm::dbgs() << "Mixed instructions in block: "
                                << *bb->getTerminator() << '\n';);
        break;
      }

      // and this block is only payload

      selected.insert(bb);
    }
  }

  Blocks.append(selected.begin(), selected.end());
}

} // namespace atrox

