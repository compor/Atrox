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

        auto *ci = llvm::dyn_cast<llvm::CallInst>(&i);
        if (isReadByCall(V, ci)) {
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

        auto *ci = llvm::dyn_cast<llvm::CallInst>(&i);
        if (isWrittenByCall(V, ci)) {
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

bool MemoryAccessInfo::isReadByCall(llvm::Value *V, llvm::CallInst *CI) {
  if (CI == nullptr) {
    return false;
  }

  if (CI->doesNotAccessMemory()) {
    return false;
  }

  auto *CalledFunction = CI->getCalledFunction();

  switch (AA->getModRefBehavior(CalledFunction)) {
  case llvm::FMRB_UnknownModRefBehavior:
    llvm_unreachable("unhandled mod ref behaviour");
  case llvm::FMRB_DoesNotAccessMemory:
    return false;
  case llvm::FMRB_DoesNotReadMemory:
    return false;
  case llvm::FMRB_OnlyAccessesInaccessibleMem:
  case llvm::FMRB_OnlyAccessesInaccessibleOrArgMem:
    llvm_unreachable("unhandled mod ref behaviour");
    // LLVM_DEBUG(llvm::dbgs() << "unhandled mod ref behaviour\n");
    return false;
  case llvm::FMRB_OnlyReadsMemory:
    // TODO possible read from globals
    return true;
  case llvm::FMRB_OnlyReadsArgumentPointees:
  case llvm::FMRB_OnlyAccessesArgumentPointees: {
    for (const auto &Arg : CI->arg_operands()) {
      if (!Arg->getType()->isPointerTy()) {
        continue;
      }

      if (Arg == V) {
        return true;
      }
    }

    return false;
  }
  }

  // TODO we need to refine this to account for aliasing in the function via
  // other ways
  return true;
}

bool MemoryAccessInfo::isWrittenByCall(llvm::Value *V, llvm::CallInst *CI) {
  if (CI == nullptr) {
    return false;
  }

  if (CI->doesNotAccessMemory()) {
    return false;
  }

  auto *CalledFunction = CI->getCalledFunction();

  switch (AA->getModRefBehavior(CalledFunction)) {
  case llvm::FMRB_UnknownModRefBehavior:
    llvm_unreachable("unhandled mod ref behaviour");
  case llvm::FMRB_DoesNotAccessMemory:
    return false;
  case llvm::FMRB_DoesNotReadMemory:
    return false;
  case llvm::FMRB_OnlyAccessesInaccessibleMem:
  case llvm::FMRB_OnlyAccessesInaccessibleOrArgMem:
    llvm_unreachable("unhandled mod ref behaviour");
    // LLVM_DEBUG(llvm::dbgs() << "unhandled mod ref behaviour\n");
    return false;
  case llvm::FMRB_OnlyReadsMemory:
    return false;
  case llvm::FMRB_OnlyReadsArgumentPointees:
    return false;
  case llvm::FMRB_OnlyAccessesArgumentPointees: {
    for (const auto &Arg : CI->arg_operands()) {
      if (!Arg->getType()->isPointerTy()) {
        continue;
      }

      if (Arg == V) {
        return true;
      }
    }

    // TODO refine here if the value passed is aliasing the tested value?
    return false;
  }
  }

  // TODO we need to refine this to account for aliasing in the function via
  // other ways
  return true;
}

} // namespace atrox

