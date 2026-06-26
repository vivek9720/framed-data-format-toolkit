#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "deserializer.h"
#include "parser.h"
#include "validator.h"

namespace {

std::vector<uint8_t> readFile(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("unable to open input file: " + path);
  }
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(input),
                              std::istreambuf_iterator<char>());
}

const char* severityName(fdf::Severity severity) {
  switch (severity) {
    case fdf::Severity::Info:
      return "info";
    case fdf::Severity::Warning:
      return "warning";
    case fdf::Severity::Error:
      return "error";
  }
  return "unknown";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: fdf_inspect <file.fdf>\n";
    return 2;
  }

  try {
    std::vector<uint8_t> bytes = readFile(argv[1]);
    fdf::ParserOptions options;
    options.strict_checksums = false;

    fdf::Parser parser;
    fdf::ParsedFile parsed = parser.parse(bytes, options);
    fdf::Document document = fdf::Deserializer().build(parsed);
    fdf::ValidationReport report = fdf::Validator().validate(parsed);

    std::cout << "FDF " << static_cast<int>(document.major_version) << "."
              << static_cast<int>(document.minor_version) << "\n";
    std::cout << "strings: " << document.strings.size() << "\n";
    std::cout << "metadata: " << document.metadata.size() << "\n";
    std::cout << "commands: " << document.commands.size() << "\n";
    std::cout << "events: " << document.events.size() << "\n";
    std::cout << "data blocks: " << document.data_blocks.size() << "\n";

    for (const fdf::ValidationIssue& issue : report.issues) {
      std::cout << severityName(issue.severity) << ": " << issue.code << ": "
                << issue.message << " @" << issue.offset << "\n";
    }
    return report.ok ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << "parse failed: " << error.what() << "\n";
    return 1;
  }
}
