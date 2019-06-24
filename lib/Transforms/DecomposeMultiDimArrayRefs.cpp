//
//
//

#include "Atrox/Transforms/DecomposeMultiDimArrayRefs.hpp"

#include "llvm/IR/Value.h"
// using llvm::Value

#include "llvm/IR/Instruction.h"
// using llvm::Instruction

#include "llvm/IR/Instructions.h"
// using llvm::CastInst
// using llvm::GetElementPtrInst

#include "llvm/IR/Instructions.h"
// using llvm::Type
// using llvm::ArrayType
// using llvm::PointerType

#include "llvm/ADT/ArrayRef.h"
// using llvm::ArrayRef

#include "llvm/Support/Debug.h"
// using LLVM_DEBUG macro
// using llvm::dbgs
// using llvm::errs

#include <vector>
// using std::vector

#define DEBUG_TYPE "atrox-decomp-array-refs"

namespace atrox {

bool DecomposeMultiDimArrayRefs(llvm::GetElementPtrInst *GEP) {
  assert(GEP && "GEP instruction is null!");

  LLVM_DEBUG(llvm::dbgs() << "decomposing: " << *GEP << '\n';);

  if (GEP->getNumIndices() < 2) {
    LLVM_DEBUG(llvm::dbgs() << "GEP instruction already has 1 index\n";);
    return false;
  }

  llvm::Value *lastPtr = GEP->getPointerOperand();
  llvm::Instruction *insertPoint = GEP;

  unsigned levels = 0;
  llvm::Type *basicTy = nullptr;
  auto *elemTy = llvm::dyn_cast<llvm::PointerType>(GEP->getPointerOperandType())
                     ->getElementType();
  levels++;

  while (auto *arrayTy = llvm::dyn_cast<llvm::ArrayType>(elemTy)) {
    basicTy = elemTy = arrayTy->getElementType();
    levels++;
  }

  if (levels != GEP->getNumIndices()) {
    // TODO check the relation with other composite type e.g. structs
    LLVM_DEBUG(llvm::dbgs() << "GEP instruction does not step through arrays "
                               "in all its indices\n";);
    return false;
  }

  LLVM_DEBUG(llvm::dbgs() << "basic type: " << *basicTy << '\n'
                          << "indirection levels: " << levels << '\n';);

  auto *ptrTy = llvm::PointerType::getUnqual(basicTy);
  std::vector<llvm::PointerType *> newTypes{ptrTy};

  for (auto i = 1u; i < levels; ++i) {
    newTypes.push_back(llvm::PointerType::getUnqual(newTypes.back()));
  }

  std::reverse(newTypes.begin(), newTypes.end());

  lastPtr = llvm::CastInst::CreatePointerCast(lastPtr, newTypes[0], "ptrcast",
                                              insertPoint);
  std::vector<llvm::Value *> newInsts;
  newInsts.push_back(lastPtr);

  auto idx = GEP->idx_begin();
  for (auto i = 0u; i < GEP->getNumIndices(); ++i, ++idx) {
    llvm::ArrayRef<llvm::Value *> indices{*idx};

    lastPtr =
        llvm::GetElementPtrInst::Create(newTypes[i]->getElementType(), lastPtr,
                                        indices, "lastptr", insertPoint);
    newInsts.push_back(lastPtr);

    // do not load the contents of the last ptr,
    // since we are replacing its uses later
    if (i < GEP->getNumIndices() - 1) {
      lastPtr = new llvm::LoadInst(lastPtr, "ptrload", insertPoint);
      newInsts.push_back(lastPtr);
    }
  }

  LLVM_DEBUG(for (auto *e : newInsts) { llvm::dbgs() << *e << '\n'; };);

  GEP->replaceAllUsesWith(newInsts.back());
  GEP->eraseFromParent();

  return true;
}

} // namespace atrox

