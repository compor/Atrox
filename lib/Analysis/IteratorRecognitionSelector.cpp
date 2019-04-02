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

#include "llvm/Analysis/LoopIterator.h"
// using llvm::LoopBlocksRPO

#include "llvm/IR/Function.h"
// using llvm::Function

#include "llvm/ADT/SetVector.h"
// usiing llvm::SmallSetVector

#include "llvm/Support/Debug.h"
// using LLVM_DEBUG macro
// using llvm::dbgs

#define DEBUG_TYPE "atrox-selector-itr"

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

  Info = std::make_unique<decltype(Info)::element_type>(LI, *pdgraph);
}

void IteratorRecognitionSelector::calculate(llvm::Loop &L) {
  llvm::LoopBlocksRPO RPOTraversal(&L);
  RPOTraversal.perform(CurLI);
  llvm::SmallVector<llvm::BasicBlock *, 16> blocks{RPOTraversal.begin(),
                                                   RPOTraversal.end()};
  llvm::SmallSetVector<llvm::BasicBlock *, 16> selected;

  for (auto *bb : blocks) {
    if (selected.count(bb)) {
      continue;
    }

    if (bb == L.getHeader()) {
      selected.insert(bb);
    } else if (CurLI->isLoopHeader(bb)) {
      // and is all payload

      llvm::LoopBlocksRPO innerRPOTraversal(CurLI->getLoopFor(bb));
      innerRPOTraversal.perform(CurLI);

      for (auto *e : innerRPOTraversal) {
        selected.insert(e);
      }
    } else if (true) {
      // if it is all payload
      selected.insert(bb);
    } else {
      // for now stop at the first sign of iterator
      LLVM_DEBUG(llvm::dbgs() << "stopping at basic block: "
                              << *bb->getTerminator() << '\n';);
      break;
    }

    Blocks.append(selected.begin(), selected.end());
  }
}

} // namespace atrox

