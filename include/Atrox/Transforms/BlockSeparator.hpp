//
//
//

#pragma once

#include "Atrox/Config.hpp"

#if DECOUPLELOOPSFRONT_USES_DECOUPLELOOPS
#include "DecoupleLoops.h"
#endif // DECOUPLELOOPSFRONT_USES_DECOUPLELOOPS

#include "llvm/IR/InstVisitor.h"
// using llvm::InstVisitor

#include "llvm/ADT/StringRef.h"
// using llvm::StringRef

#include <map>
// using std::map

#include <vector>
// using std::vector

#include <utility>
// using std::pair

namespace llvm {
class Instruction;
class BasicBlock;
class LoopInfo;
class Loop;
class DominatorTree;
} // namespace llvm

namespace iteratorrecognition {
class IteratorInfo;
} // namespace iteratorrecognition

namespace atrox {

enum class Mode : unsigned { Iterator, Payload };

template <typename T> using ModeMapTy = std::map<T, Mode>;
template <typename T> using ModeChangePointTy = std::pair<T, Mode>;
template <typename T, typename Y>
using ModeChangePointMapTy = std::map<T, std::vector<ModeChangePointTy<Y>>>;

using BlockModeMapTy = ModeMapTy<llvm::BasicBlock *>;
using BlockModeChangePointTy = ModeChangePointTy<llvm::Instruction *>;
using BlockModeChangePointMapTy =
    ModeChangePointMapTy<llvm::BasicBlock *, llvm::Instruction *>;

inline Mode InvertMode(Mode m) {
  return m == Mode::Payload ? Mode::Iterator : Mode::Payload;
}

inline Mode GetMode(const llvm::Instruction &Inst, const llvm::Loop &CurLoop,
                    const iteratorrecognition::IteratorInfo &Info);

bool FindPartitionPoints(const llvm::Loop &CurLoop,
                         const iteratorrecognition::IteratorInfo &Info,
                         BlockModeMapTy &Modes,
                         BlockModeChangePointMapTy &Points);

void SplitAtPartitionPoints(BlockModeChangePointMapTy &Points,
                            BlockModeMapTy &Modes,
                            llvm::DominatorTree *DT = nullptr,
                            llvm::LoopInfo *LI = nullptr);

} // namespace atrox

