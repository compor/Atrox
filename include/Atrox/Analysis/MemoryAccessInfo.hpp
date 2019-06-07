//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "llvm/ADT/SmallVector.h"
// using llvm::SmallVector

#include "llvm/ADT/ArrayRef.h"
// using llvm::ArrayRef

#include <cassert>
// using assert

namespace llvm {
class Value;
class BasicBlock;
class Instruction;
class AAResults;
} // namespace llvm

namespace atrox {

class MemoryAccessInfo {
  llvm::SmallVector<llvm::BasicBlock *, 16> Blocks;
  llvm::AAResults *AA;

public:
  explicit MemoryAccessInfo(llvm::ArrayRef<llvm::BasicBlock *> TargetBlocks,
                            llvm::AAResults *AA)
      : Blocks{TargetBlocks.begin(), TargetBlocks.end()}, AA{AA} {}

  bool isRead(llvm::Value *V);
  bool isWrite(llvm::Value *V);
};

} // namespace atrox

