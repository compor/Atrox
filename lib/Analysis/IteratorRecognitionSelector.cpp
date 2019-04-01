//
//
//

#include "Atrox/Analysis/IteratorRecognitionSelector.hpp"

#include "Pedigree/Analysis/Creational/DDGraphBuilder.hpp"

#include "Pedigree/Analysis/Creational/CDGraphBuilder.hpp"

#include "Pedigree/Analysis/Creational/MDAMDGraphBuilder.hpp"

#include "Pedigree/Support/Utils/InstIterator.hpp"

#include "llvm/Analysis/LoopInfo.h"
// using llvm::LoopInfo
// using llvm::Loop

#include "llvm/Analysis/MemoryDependenceAnalysis.h"
// using llvm::MemoryDependenceResults

#include "llvm/IR/Function.h"
// using llvm::Function

namespace atrox {

IteratorRecognitionSelector::IteratorRecognitionSelector(
    llvm::Function &Func, llvm::LoopInfo &LI, llvm::MemoryDependenceResults *MD)
    : CurFunc(&Func), CurLI(&LI), CurMD(MD) {
  pedigree::DDGraphBuilder ddgBuilder{};
  auto ddgraph = ddgBuilder.setUnit(Func).ignoreConstantPHINodes(true).build();

  pedigree::CDGraphBuilder cdgBuilder{};
  auto cdgraph = cdgBuilder.setUnit(Func).build();

  pedigree::MDAMDGraphBuilder mdgBuilder{};
  auto mdgraph = mdgBuilder.setAnalysis(*CurMD).build(
      pedigree::make_inst_begin(Func), pedigree::make_inst_end(Func));
}

void IteratorRecognitionSelector::calculate(llvm::Loop &L) {}

} // namespace atrox

