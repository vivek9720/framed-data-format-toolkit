#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "deserializer.h"
#include "format.h"

namespace fdf {

enum class Severity {
  Info,
  Warning,
  Error,
};

struct ValidationIssue {
  Severity severity = Severity::Info;
  std::string code;
  std::string message;
  size_t offset = 0;
};

struct ValidationReport {
  bool ok = true;
  std::vector<ValidationIssue> issues;
};

class Validator {
 public:
  ValidationReport validate(const ParsedFile& parsed) const;
  ValidationReport validate(const Document& document) const;

 private:
  void addIssue(ValidationReport& report, Severity severity, std::string code,
                std::string message, size_t offset = 0) const;
  void validateValue(const ParsedFile& parsed, const MetadataValue& value,
                     size_t offset, ValidationReport& report) const;
  void validateCommand(const ParsedFile& parsed, const CommandRecord& command,
                       size_t depth, ValidationReport& report) const;
};

}  // namespace fdf
