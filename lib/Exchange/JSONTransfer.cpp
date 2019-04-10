//
//
//

#include "Atrox/Exchange/JSONTransfer.hpp"

#include <utility>
// using std::move

namespace llvm {
namespace json {

Value toJSON(ArrayRef<atrox::ArgSpec> ArgSpecs) {
  Object root;
  Array specs;

  for (const auto &s : ArgSpecs) {
    Object item;
    item["direction"] = toInt(s.Direction);
    item["iterator dependent"] = s.IteratorDependent;

    specs.push_back(std::move(item));
  }

  root["argspecs"] = std::move(specs);

  return std::move(root);
}

} // namespace json
} // namespace llvm

