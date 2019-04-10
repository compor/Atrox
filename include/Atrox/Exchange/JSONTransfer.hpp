//
//
//

#pragma once

#include "Atrox/Config.hpp"

#include "Atrox/Support/IR/ArgSpec.hpp"

#include "llvm/Support/JSON.h"
// using json::Value
// using json::Object
// using json::Array

namespace llvm {
namespace json {

Value toJSON(const atrox::ArgSpec AS);

} // namespace json
} // namespace llvm

