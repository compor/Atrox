//
//
//

#include "Atrox/Config.hpp"

#include "llvm/IR/Module.h"
// using llvm::Module

#include "llvm/Analysis/LoopInfo.h"
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

  template <typename T> bool clone(llvm::Loop &CurLoop) {
    T selector{CurLoop};

    auto blocks = selector.getBlocks();

    if (blocks.empty()) {
      LLVM_DEBUG(llvm::dbgs()
                 << "Skipping loop because no blocks were selected.\n");
      return false;
    }

    return false;
  }
};

} // namespace atrox

#undef DEBUG_TYPE

