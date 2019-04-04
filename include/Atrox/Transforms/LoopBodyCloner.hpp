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

#include "llvm/Support/Debug.h"
// using DEBUG macro
// using llvm::dbgs

#define DEBUG_TYPE "atrox-loop-body-clone"

namespace atrox {

class LoopBodyCloner {
  llvm::Module *TargetModule;

public:
  explicit LoopBodyCloner(llvm::Module &CurM) : TargetModule(&CurM) {}

  template <typename T> bool cloneLoops(llvm::LoopInfo &LI, T &Selector) {
    bool hasChanged = false;

    for (auto &curLoop : LI) {
      llvm::SmallVector<llvm::BasicBlock *, 32> blocks;
      Selector.getBlocks(*curLoop, blocks);

      if (blocks.empty()) {
        LLVM_DEBUG(llvm::dbgs()
                   << "Skipping loop because no blocks were selected.\n");
        continue;
      }

      atrox::CodeExtractor ce{blocks};
      auto *extractedFunc = ce.cloneCodeRegion();
      hasChanged |= extractedFunc ? true : false;
    }

    return hasChanged;
  }
};

} // namespace atrox

#undef DEBUG_TYPE

