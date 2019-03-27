//
//
//

#include "Atrox/Analysis/NaiveSelector.hpp"

#include "llvm/Analysis/LoopInfo.h"
// using llvm::Loop

namespace atrox {

void NaiveSelector::calculate() {
  auto *hdr = CurLoop->getHeader();

  Blocks.clear();

  for (auto *e : CurLoop->getBlocks()) {
    if (e == hdr || CurLoop->isLoopLatch(e)) {
      continue;
    }

    Blocks.push_back(e);
  }
}

} // namespace atrox

