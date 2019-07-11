//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "llvm/IR/Type.h"

#include "llvm/ADT/SmallVector.h"

namespace llvm {
class GetElementPtrInst;
} // namespace llvm

namespace atrox {

llvm::Type *GetDecomposedMultiDimArrayType(
    const llvm::PointerType *PtrTy,
    llvm::SmallVectorImpl<llvm::Type *> *NewTypes = nullptr);

void GetDecomposedArraySizes(const llvm::PointerType *PtrTy,
                             llvm::SmallVectorImpl<uint64_t> &Sizes);

bool FlattenMultiDimArrayIndices(llvm::GetElementPtrInst *GEP);

bool DecomposeMultiDimArrayRefs(llvm::GetElementPtrInst *GEP);

} // namespace atrox

