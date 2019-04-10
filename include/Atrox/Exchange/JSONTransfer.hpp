//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "Atrox/Support/IR/ArgSpec.hpp"

#include "llvm/ADT/ArrayRef.h"
// usign llvm::ArrayRef

#include "llvm/Support/JSON.h"
// using json::Value
// using json::Object
// using json::Array

namespace atrox {

void WriteJSONToFile(const llvm::json::Value &V,
                     const llvm::Twine &FilenamePrefix, const llvm::Twine &Dir);

} // namespace atrox

//

namespace llvm {
namespace json {

Value toJSON(ArrayRef<atrox::ArgSpec> ArgSpecs);

} // namespace json
} // namespace llvm

