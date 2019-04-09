//
//
//

#include "private/ITRUtils.hpp"

namespace atrox {

std::unique_ptr<iteratorrecognition::IteratorRecognitionInfo>
BuildITRInfo(const llvm::LoopInfo &LI, pedigree::PDGraph &PDG) {
  return std::make_unique<iteratorrecognition::IteratorRecognitionInfo>(LI, PDG);
}

} // namespace atrox

