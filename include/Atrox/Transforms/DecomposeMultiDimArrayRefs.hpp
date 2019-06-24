//
//
//

#pragma once

#include "Atrox/Config.hpp"

namespace llvm {
class GetElementPtrInst;
} // namespace llvm

namespace atrox {

bool DecomposeMultiDimArrayRefs(llvm::GetElementPtrInst *GEP);

} // namespace atrox

