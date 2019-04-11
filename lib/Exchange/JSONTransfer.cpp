//
//
//

#include "Atrox/Exchange/JSONTransfer.hpp"

#include "llvm/Support/FileSystem.h"
// using llvm::sys::fs::F_Text

#include "llvm/IR/Function.h"
// using llvm::Function

#include "llvm/Support/Path.h"
// using llvm::sys::path::filename

#include "llvm/Support/ToolOutputFile.h"
// using llvm::ToolOutputFile

#include "llvm/Support/Debug.h"
// using LLVM_DEBUG macro
// using llvm::dbgs
// using llvm::errs

#include <utility>
// using std::move

#include <system_error>
// using std::error_code

namespace atrox {

void WriteJSONToFile(const llvm::json::Value &V,
                     const llvm::Twine &FilenamePrefix,
                     const llvm::Twine &Dir) {
  std::string absFilename{Dir.str() + "/" + FilenamePrefix.str() + ".json"};
  llvm::StringRef filename{llvm::sys::path::filename(absFilename)};
  llvm::errs() << "Writing file '" << filename << "'... ";

  std::error_code ec;
  llvm::ToolOutputFile of(absFilename, ec, llvm::sys::fs::F_Text);

  if (ec) {
    llvm::errs() << "error opening file '" << filename << "' for writing!\n";
    of.os().clear_error();
  }

  of.os() << llvm::formatv("{0:2}", V);
  of.os().close();

  if (!of.os().has_error()) {
    of.keep();
  }

  llvm::errs() << " done. \n";
}

} // namespace atrox

//

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

Value toJSON(const atrox::FunctionArgSpec &FAS) {
  Object root;

  root["func"] = FAS.Func->getName();
  root["args"] = toJSON(FAS.Args);

  return std::move(root);
}

} // namespace json
} // namespace llvm

