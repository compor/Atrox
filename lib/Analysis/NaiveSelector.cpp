//
//
//

#include "Atrox/Analysis/NaiveSelector.hpp"

#include "llvm/Analysis/LoopInfo.h"
// using llvm::Loop

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

