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

namespace llvm {
namespace json {

Value toJSON(ArrayRef<atrox::ArgSpec> ArgSpecs);

} // namespace json
} // namespace llvm

