//
//
//

#include "Atrox/Support/IR/GeneralUtils.hpp"

#include "llvm/IR/Value.h"
// using llvm::Value

#include "llvm/IR/IntrinsicInst.h"
// using llvm::IntrinsicInst

#include "llvm/Support/Debug.h"
// using LLVM_DEBUG macro
// using llvm::dbgs

#include <algorithm>
// using std::for_each

#define DEBUG_TYPE "atrox-general-utils"

namespace atrox {

bool IsIgnoredIntrinsic(const llvm::Value *V) {
  if (auto *IT = llvm::dyn_cast<llvm::IntrinsicInst>(V)) {
    switch (IT->getIntrinsicID()) {
    // Lifetime markers are supported/ignored.
    case llvm::Intrinsic::lifetime_start:
    case llvm::Intrinsic::lifetime_end:
    // Invariant markers are supported/ignored.
    case llvm::Intrinsic::invariant_start:
    case llvm::Intrinsic::invariant_end:
    // Some misc annotations are supported/ignored.
    case llvm::Intrinsic::var_annotation:
    case llvm::Intrinsic::ptr_annotation:
    case llvm::Intrinsic::annotation:
    case llvm::Intrinsic::donothing:
    case llvm::Intrinsic::assume:
    // Some debug info intrinsics are supported/ignored.
    case llvm::Intrinsic::dbg_value:
    case llvm::Intrinsic::dbg_declare:
      return true;
    default:
      break;
    }
  }

  return false;
}

//

void InstructionEraser::visitIntrinsicInst(llvm::IntrinsicInst &I) {
  if (IsIgnoredIntrinsic(&I)) {
    ToRemove.push_back(&I);
  }
}

bool InstructionEraser::process() {
  bool hasChanged = !ToRemove.empty();

  std::for_each(ToRemove.begin(), ToRemove.end(),
                [](auto *e) { e->eraseFromParent(); });

  return hasChanged;
}

} // namespace atrox

