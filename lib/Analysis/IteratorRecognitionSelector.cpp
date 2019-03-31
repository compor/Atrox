//
//
//

#include "Atrox/Analysis/IteratorRecognitionSelector.hpp"

#include "Pedigree/Analysis/Creational/DDGraphBuilder.hpp"

#include "llvm/Analysis/LoopInfo.h"
// using llvm::LoopInfo
// using llvm::Loop

#include "llvm/IR/Function.h"
// using llvm::Function

namespace atrox {

IteratorRecognitionSelector::IteratorRecognitionSelector(llvm::Function &Func,
                                                         llvm::LoopInfo &LI)
    : CurFunc(&Func), CurLI(&LI) {
  pedigree::DDGraphBuilder ddgBuilder{};
  auto ddgraph = ddgBuilder.setUnit(Func).ignoreConstantPHINodes(true).build();
}

void IteratorRecognitionSelector::calculate(llvm::Loop &L) {}

} // namespace atrox

