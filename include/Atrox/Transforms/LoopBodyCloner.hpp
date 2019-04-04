//
//
//

#include "Atrox/Config.hpp"

#include "Atrox/Transforms/Utils/CodeExtractor.hpp"

#include "llvm/IR/Module.h"
// using llvm::Module

#include "llvm/IR/Function.h"
// using llvm::Function

#include "llvm/IR/Dominators.h"
// using llvm::DominatorTree

#include "llvm/Analysis/LoopInfo.h"
// using llvm::LoopInfo
// using llvm::Loop

#include "llvm/ADT/SmallVector.h"
// using llvm::SmallVector

#include "llvm/Support/Debug.h"
// using DEBUG macro
// using llvm::dbgs

#define DEBUG_TYPE "atrox-loop-body-clone"

namespace atrox {

class LoopBodyCloner {
  llvm::Module *TargetModule;

public:
  explicit LoopBodyCloner(llvm::Module &CurM) : TargetModule(&CurM) {}

  template <typename T>
  bool cloneLoop(llvm::Loop &L, llvm::LoopInfo &LI, T &Selector) {
    bool hasChanged = false;

    llvm::SmallVector<llvm::BasicBlock *, 32> blocks;
    Selector.getBlocks(L, blocks);

    if (!blocks.empty()) {
      atrox::CodeExtractor ce{blocks};
      auto *extractedFunc = ce.cloneCodeRegion();
      hasChanged |= extractedFunc ? true : false;
    } else {
      LLVM_DEBUG(llvm::dbgs()
                 << "Skipping loop because no blocks were selected.\n");
    }

    return hasChanged;
  }

  template <typename T> bool cloneLoops(llvm::LoopInfo &LI, T &Selector) {
    bool hasChanged = false;

    for (auto *curLoop : LI) {
      hasChanged |= cloneLoop(*curLoop, LI, Selector);
    }

    return hasChanged;
  }
};

} // namespace atrox

#undef DEBUG_TYPE

