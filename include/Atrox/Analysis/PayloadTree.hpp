//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "llvm/Analysis/LoopInfo.h"
// using llvm::LoopInfo

#include "llvm/ADT/SmallPtrSet.h"
// using llvm::SmallPtrSet

#include "llvm/ADT/SmallVector.h"
// using llvm::SmallVectorImpl

#include <map>
// using std::map

namespace llvm {
class BasicBlock;
} // namespace llvm

namespace atrox {

using BlockSetTy = llvm::SmallPtrSet<llvm::BasicBlock *, 16>;
using BlockRootGroupMapTy = std::map<llvm::BasicBlock *, BlockSetTy>;

BlockRootGroupMapTy
SelectPayloadTrees(llvm::Loop &CurLoop, llvm::LoopInfo &CurLI,
                   llvm::SmallVectorImpl<llvm::BasicBlock *> &PayloadBlocks);

} // namespace atrox

