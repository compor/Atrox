//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "Pedigree/Analysis/Graphs/PDGraph.hpp"

#include "IteratorRecognition/Analysis/IteratorRecognition.hpp"

#include <memory>
// using std::unique_ptr

namespace llvm {
class LoopInfo;
} // namespace llvm

namespace atrox {

std::unique_ptr<iteratorrecognition::IteratorRecognitionInfo> BuildITRInfo(
    const llvm::LoopInfo &LI, pedigree::PDGraph &PDG);

} // namespace atrox

