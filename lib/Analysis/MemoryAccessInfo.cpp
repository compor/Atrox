//
//
//

#include "Atrox/Analysis/MemoryAccessInfo.hpp"

#include "llvm/Analysis/AliasAnalysis.h"
// using llvm::AAResults

#include "llvm/Analysis/MemoryLocation.h"
// using llvm::MemoryLocation

#include "llvm/ADT/SmallPtrSet.h"
// using llvm::SmallPtrSet

#include "llvm/Support/Debug.h"
// using LLVM_DEBUG macro
// using llvm::dbgs

#include <algorithm>
// using std::find

#define DEBUG_TYPE "atrox-mai"

namespace atrox {

bool MemoryAccessInfo::isRead(llvm::Value *V) {
  auto *inst = llvm::dyn_cast<llvm::Instruction>(V);

  for (auto *bb : Blocks) {
    for (auto &i : *bb) {
      auto mri = AA->getModRefInfo(&i, llvm::MemoryLocation::getOrNone(inst));

      if (llvm::isRefSet(mri) && !llvm::isModSet(mri)) {
        return true;
      }
    }
  }

  return false;
}

bool MemoryAccessInfo::isWrite(llvm::Value *V) {
  auto *inst = llvm::dyn_cast<llvm::Instruction>(V);

  for (auto *bb : Blocks) {
    for (auto &i : *bb) {
      auto mri = AA->getModRefInfo(&i, llvm::MemoryLocation::getOrNone(inst));
      if (llvm::isModSet(mri)) {
        return true;
      }
    }
  }

  return false;
}

} // namespace atrox

