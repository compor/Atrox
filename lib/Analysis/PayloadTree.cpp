//
//
//

#include "Atrox/Analysis/PayloadTree.hpp"

#include "llvm/Analysis/LoopInfo.h"
// using llvm::Loop

#include "llvm/IR/CFG.h"
// using llvm::pred_begin
// using llvm::pred_end
// using llvm::succ_begin
// using llvm::succ_end

#include "llvm/ADT/SmallPtrSet.h"
// using llvm::SmallPtrSet

#include "llvm/ADT/iterator_range.h"
// using llvm::make_range

#include "llvm/Support/Debug.h"
// using LLVM_DEBUG macro
// using llvm::dbgs

#include <cassert>
// using assert

#define DEBUG_TYPE "atrox-payload-tree"

namespace atrox {

BlockRootGroupMapTy SelectPayloadTrees(
    llvm::Loop &CurLoop, const llvm::LoopInfo &CurLI,
    const llvm::SmallVectorImpl<llvm::BasicBlock *> &PayloadBlocks) {
  BlockSetTy loopBlocks;
  BlockSetTy payloadBlocks;
  BlockSetTy iteratorBlocks;
  BlockSetTy blacklistedBlocks;
  BlockSetTy rootBlocks;
  BlockRootGroupMapTy rootGroups;

  assert(CurLI.getLoopFor(CurLoop.getHeader()) == &CurLoop &&
         "CurLoop does not belong to this LoopInfo!");

  for (auto *e : PayloadBlocks) {
    payloadBlocks.insert(e);
  }

  for (auto *e : llvm::make_range(CurLoop.block_begin(), CurLoop.block_end())) {
    loopBlocks.insert(e);

    if (!payloadBlocks.count(e)) {
      iteratorBlocks.insert(e);
      blacklistedBlocks.insert(e);
    }
  }

  blacklistedBlocks.insert(CurLoop.getLoopLatch());
  blacklistedBlocks.insert(CurLoop.getHeader());

  for (auto *bb : iteratorBlocks) {
    if (CurLI.isLoopHeader(bb) &&
        CurLI.getLoopDepth(bb) > CurLoop.getLoopDepth()) {
      auto *loop = CurLI.getLoopFor(bb);
      for (auto bi = loop->block_begin(), be = loop->block_end(); be != bi;
           ++bi) {
        LLVM_DEBUG(llvm::errs() << "blacklisting inner loop blocks: "
                                << (*bi)->getName() << '\n');
        blacklistedBlocks.insert(*bi);
      }
    }
  }

  for (auto *e : payloadBlocks)
    if (!blacklistedBlocks.count(e)) {
      for (auto it = llvm::pred_begin(e), eit = llvm::pred_end(e); it != eit;
           ++it)
        if (iteratorBlocks.count(*it)) {
          rootBlocks.insert(e);
          BlockSetTy initial_set;
          initial_set.insert(e);
          rootGroups.emplace(e, initial_set);
          break;
        }
    }

  for (auto &e : rootGroups) {
    BlockSetTy worklist;
    worklist.insert(e.first);

    while (!worklist.empty()) {
      auto *bb = *worklist.begin();
      worklist.erase(bb);
      blacklistedBlocks.insert(bb);
      e.second.insert(bb);

      for (auto it = llvm::succ_begin(bb), eit = llvm::succ_end(bb); it != eit;
           ++it)
        if (!blacklistedBlocks.count(*it) && payloadBlocks.count(*it))
          worklist.insert(*it);
    }
  }

  LLVM_DEBUG(llvm::errs() << "payload roots for loop with latch: "
                          << CurLoop.getLoopLatch()->getName() << '\n');
  LLVM_DEBUG({
    for (auto &e : rootGroups) {
      llvm::errs() << e.first->getName() << '\n';
      for (auto *k : e.second)
        llvm::errs() << '\t' << k->getName() << '\n';
    }
  });

  return rootGroups;
}

} // namespace atrox

