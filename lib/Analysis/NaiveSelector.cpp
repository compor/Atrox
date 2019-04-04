//
//
//

#include "Atrox/Analysis/NaiveSelector.hpp"

#include "llvm/Analysis/LoopInfo.h"
// using llvm::Loop

#include "llvm/Support/Debug.h"
// using LLVM_DEBUG macro
// using llvm::dbgs

#define DEBUG_TYPE "atrox-selector"

namespace atrox {

void NaiveSelector::calculate(
    llvm::Loop &L, llvm::SmallVectorImpl<llvm::BasicBlock *> &Blocks) {
  auto *hdr = L.getHeader();

  for (auto *e : L.getBlocks()) {
    if (e == hdr || L.isLoopLatch(e)) {
      continue;
    }

    Blocks.push_back(e);
  }
}

} // namespace atrox

