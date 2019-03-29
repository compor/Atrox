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

  template <typename T> bool cloneLoops(llvm::Function &F) {
    llvm::LoopInfo LI{llvm::DominatorTree(const_cast<llvm::Function &>(F))};
    bool hasChanged = false;

    for (auto &curLoop : LI) {
      T selector{curLoop};

      auto blocks = selector.getBlocks();

      if (blocks.empty()) {
        LLVM_DEBUG(llvm::dbgs()
                   << "Skipping loop because no blocks were selected.\n");
        return hasChanged;
      }

      llvm::CodeExtractor ce{blocks};
    }

    return hasChanged;
  }
};

} // namespace atrox

#undef DEBUG_TYPE

