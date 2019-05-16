//
//
//

#include "Atrox/Analysis/WeightedIteratorRecognitionSelector.hpp"

#include "Atrox/Analysis/PayloadWeights.hpp"

#include "Atrox/Analysis/PayloadTree.hpp"

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
// using std::max_element

#define DEBUG_TYPE "atrox-selector"

namespace atrox {

WeightedIteratorRecognitionSelector::WeightedIteratorRecognitionSelector(
    iteratorrecognition::IteratorRecognitionInfo &ITRInfo)
    : CurLI(const_cast<llvm::LoopInfo *>(&ITRInfo.getLoopInfo())),
      Info(ITRInfo) {}

void WeightedIteratorRecognitionSelector::calculate(
    llvm::Loop &L, llvm::SmallVectorImpl<llvm::BasicBlock *> &Blocks) {
  llvm::LoopBlocksRPO RPOTraversal(&L);
  RPOTraversal.perform(CurLI);
  llvm::SmallVector<llvm::BasicBlock *, 16> blocks{RPOTraversal.begin(),
                                                   RPOTraversal.end()};
  llvm::SmallSetVector<llvm::BasicBlock *, 16> selected;

  auto infoOrError = Info.getIteratorInfoFor(&L);

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

  auto weights = CalculatePayloadWeight(payloadBlocks);
  auto trees = SelectPayloadTrees(L, Info.getLoopInfo(), payloadBlocks);

  // select payload with highest
  decltype(weights) treeWeights;
  for (auto &e : trees) {
    decltype(treeWeights)::mapped_type sum{};

    for (auto *k : e.second) {
      sum += weights[k];
    }

    treeWeights[e.first] = sum;
  }

  auto m = std::max_element(
      treeWeights.begin(), treeWeights.end(),
      [](const auto &e1, const auto &e2) { return e1.second < e2.second; });

  selected.insert(trees[m->first].begin(), trees[m->first].end());
  Blocks.append(selected.begin(), selected.end());
}

} // namespace atrox

