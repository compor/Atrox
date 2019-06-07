//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "llvm/Analysis/AliasSetTracker.h"
// using llvm::AliasSetTracker

#include "llvm/ADT/SmallVector.h"
// using llvm::SmallVector

#include <cassert>
// using assert

namespace llvm {
class Value;
class BasicBlock;
class Instruction;
class Loop;
class AAResults;
class AliasSetTracker;
} // namespace llvm

namespace atrox {

class MemoryAccessInfo {
  llvm::SmallVector<llvm::BasicBlock *, 16> Blocks;
  llvm::SmallVector<llvm::Instruction *, 16> Loads, Stores;
  llvm::SmallVector<llvm::Value *, 16> LoadPtrs, StorePtrs;
  llvm::AliasSetTracker AST;

  void filterMemoryAccesses();
  void findAccessPointers();

public:
  explicit MemoryAccessInfo(llvm::AAResults *AA) : AST{*AA} {
    filterMemoryAccesses();
    findAccessPointers();
  }

  bool isRead(llvm::Value *V);
  bool isWrite(llvm::Value *V);
};

} // namespace atrox

