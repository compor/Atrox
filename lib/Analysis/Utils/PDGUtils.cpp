//
//
//

#include "private/PDGUtils.hpp"

#include "Pedigree/Analysis/Creational/DDGraphBuilder.hpp"

#include "Pedigree/Analysis/Creational/CDGraphBuilder.hpp"

#include "Pedigree/Analysis/Creational/MDAMDGraphBuilder.hpp"

#include "Pedigree/Analysis/Creational/PDGraphBuilder.hpp"

#include "Pedigree/Support/GraphConverter.hpp"

#include "Pedigree/Support/Utils/UnitConverters.hpp"

#include "Pedigree/Support/Utils/InstIterator.hpp"

namespace atrox {

std::unique_ptr<pedigree::PDGraph> BuildPDG(llvm::Function &Func,
                                            llvm::MemoryDependenceResults *MD) {
  pedigree::DDGraphBuilder ddgBuilder{};
  auto ddgraph = ddgBuilder.setUnit(Func).ignoreConstantPHINodes(true).build();

  pedigree::CDGraphBuilder cdgBuilder{};
  auto cdgraph = cdgBuilder.setUnit(Func).build();
  auto icdgraph = std::make_unique<pedigree::InstCDGraph>();
  pedigree::Convert(*cdgraph, *icdgraph,
                    pedigree::BlockToTerminatorUnitConverter{},
                    pedigree::BlockToInstructionsUnitConverter{});

  pedigree::MDAMDGraphBuilder mdgBuilder{};
  auto mdgraph = mdgBuilder.setAnalysis(*MD).build(
      pedigree::make_inst_begin(Func), pedigree::make_inst_end(Func));

  pedigree::PDGraphBuilder builder{};

  builder.addGraph(*ddgraph).addGraph(*icdgraph).addGraph(*mdgraph);
  auto pdgraph = builder.build();

  pdgraph->connectRootNode();

  return std::move(pdgraph);
}

} // namespace atrox

