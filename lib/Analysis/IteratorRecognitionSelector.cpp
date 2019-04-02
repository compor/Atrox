//
//
//

#include "Atrox/Analysis/IteratorRecognitionSelector.hpp"

#include "Pedigree/Analysis/Creational/DDGraphBuilder.hpp"

#include "Pedigree/Analysis/Creational/CDGraphBuilder.hpp"

#include "Pedigree/Analysis/Creational/MDAMDGraphBuilder.hpp"

#include "Pedigree/Analysis/Creational/PDGraphBuilder.hpp"

#include "Pedigree/Support/GraphConverter.hpp"

#include "Pedigree/Support/Utils/UnitConverters.hpp"

#include "Pedigree/Support/Utils/InstIterator.hpp"

#include "IteratorRecognition/Analysis/IteratorRecognition.hpp"

#include "llvm/Analysis/LoopInfo.h"
// using llvm::LoopInfo
// using llvm::Loop

#include "llvm/Analysis/MemoryDependenceAnalysis.h"
// using llvm::MemoryDependenceResults

#include "llvm/IR/Function.h"
// using llvm::Function

// namespace aliases

namespace itr = iteratorrecognition;

namespace atrox {

IteratorRecognitionSelector::IteratorRecognitionSelector(
    llvm::Function &Func, llvm::LoopInfo &LI, llvm::MemoryDependenceResults *MD)
    : CurFunc(&Func), CurLI(&LI), CurMD(MD) {
  pedigree::DDGraphBuilder ddgBuilder{};
  auto ddgraph = ddgBuilder.setUnit(Func).ignoreConstantPHINodes(true).build();

  pedigree::CDGraphBuilder cdgBuilder{};
  auto cdgraph = cdgBuilder.setUnit(Func).build();
  decltype(ddgraph) icdgraph;
  pedigree::Convert(*cdgraph, *icdgraph,
                    pedigree::BlockToTerminatorUnitConverter{},
                    pedigree::BlockToInstructionsUnitConverter{});

  pedigree::MDAMDGraphBuilder mdgBuilder{};
  auto mdgraph = mdgBuilder.setAnalysis(*CurMD).build(
      pedigree::make_inst_begin(Func), pedigree::make_inst_end(Func));

  pedigree::PDGraphBuilder builder{};

  builder.addGraph(*ddgraph).addGraph(*icdgraph).addGraph(*mdgraph);
  auto pdgraph = builder.build();

  pdgraph->connectRootNode();

  itr::IteratorRecognitionInfo itrInfo{LI, *pdgraph};
}

void IteratorRecognitionSelector::calculate(llvm::Loop &L) {}

} // namespace atrox

