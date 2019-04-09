//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "Pedigree/Analysis/Graphs/PDGraph.hpp"

#include <memory>
// using std::unique_ptr

namespace llvm {
class Function;
class MemoryDependenceResults;
} // namespace llvm

namespace atrox {

std::unique_ptr<pedigree::PDGraph> BuildPDG(llvm::Function &Func,
                                            llvm::MemoryDependenceResults *MD);

} // namespace atrox

