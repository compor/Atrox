//
//
//

#include "Atrox/Analysis/MemoryAccessInfo.hpp"

#include "private/PassCommandLineOptions.hpp"

#include "llvm/Analysis/AliasAnalysis.h"
// using llvm::AAResults

#include "llvm/Analysis/MemoryLocation.h"
// using llvm::MemoryLocation

#include "llvm/ADT/SmallPtrSet.h"
// using llvm::SmallPtrSet

#include "llvm/Support/Debug.h"
// using LLVM_DEBUG macro
// using llvm::dbgs

#include "llvm/Support/Debug.h"
// using llvm_unreachable

#include <algorithm>
// using std::find

#define DEBUG_TYPE "atrox-mai"

namespace atrox {

bool MemoryAccessInfo::isRead(llvm::Value *V) {
  LLVM_DEBUG(llvm::dbgs() << "examining read for: " << *V << '\n';);

  if (auto *inst = llvm::dyn_cast<llvm::Instruction>(V)) {
    for (auto *bb : Blocks) {
      for (auto &i : *bb) {
        auto mri = AA->getModRefInfo(&i, llvm::MemoryLocation::getOrNone(inst));

        if (llvm::isRefSet(mri) && !llvm::isModSet(mri)) {
          return true;
        }
      }
    }
  } else if (auto *arg = llvm::dyn_cast<llvm::Argument>(V)) {
    if (AtroxIgnoreAliasing) {
      return true;
    }

    // TODO more elaborate handling is required here
  } else if (auto *glob = llvm::dyn_cast<llvm::GlobalVariable>(V)) {
    if (AtroxIgnoreAliasing) {
      return true;
    }

    // TODO more elaborate handling is required here
  } else {
    llvm_unreachable("Unhandled value type");
  }

  return false;
}

bool MemoryAccessInfo::isWrite(llvm::Value *V) {
  LLVM_DEBUG(llvm::dbgs() << "examining write for: " << *V << '\n';);

  if (auto *inst = llvm::dyn_cast<llvm::Instruction>(V)) {
    for (auto *bb : Blocks) {
      for (auto &i : *bb) {
        auto mri = AA->getModRefInfo(&i, llvm::MemoryLocation::getOrNone(inst));
        if (llvm::isModSet(mri)) {
          return true;
        }
      }
    }
  } else if (auto *arg = llvm::dyn_cast<llvm::Argument>(V)) {
    if (AtroxIgnoreAliasing) {
      return true;
    }

    // TODO more elaborate handling is required here
  } else if (auto *glob = llvm::dyn_cast<llvm::GlobalVariable>(V)) {
    if (AtroxIgnoreAliasing) {
      return true;
    }

    // TODO more elaborate handling is required here
  } else {
    llvm_unreachable("Unhandled value type");
  }

  return false;
}

} // namespace atrox

