//
//
//

#include "Atrox/Exchange/JSONTransfer.hpp"

#include <utility>
// using std::move

namespace llvm {
namespace json {

Value toJSON(const atrox::ArgSpec &AS) {
  Object root;

  root["direction"] = toInt(AS.Direction);
  root["iterator dependent"] = AS.IteratorDependent;

  return std::move(root);
}

} // namespace json
} // namespace llvm

