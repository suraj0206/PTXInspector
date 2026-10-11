#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

struct Instruction {
    std::string opcode;
    std::string text;
    int line;
};

static std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

static std::string removeComments(const std::string& source) {
    std::string out;
    bool lineComment = false, blockComment = false;
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (lineComment) {
            if (source[i] == '\n') {
                lineComment = false;
                out += '\n';
            }
            continue;
        }
        if (blockComment) {
            if (source[i] == '*' && i + 1 < source.size() && source[i + 1] == '/') {
                blockComment = false;
                ++i;
            } else if (source[i] == '\n') {
                out += '\n';
            }
            continue;
        }
        if (source[i] == '/' && i + 1 < source.size() && source[i + 1] == '/') {
            lineComment = true;
            ++i;
        } else if (source[i] == '/' && i + 1 < source.size() && source[i + 1] == '*') {
            blockComment = true;
            ++i;
        } else {
            out += source[i];
        }
    }
    return out;
}

static bool startsWith(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

static std::string opcodeFrom(const std::string& statement) {
    std::istringstream in(statement);
    std::string token;
    in >> token;
    if (!token.empty() && token[0] == '@') {
        in >> token; // Skip a PTX predicate, e.g. @%p1
    }
    return token;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ptx_inspector <file.ptx>\n";
        return 2;
    }

    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "Error: cannot open file: " << argv[1] << "\n";
        return 2;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string source = removeComments(buffer.str());

    std::set<std::string> labels;
    std::vector<Instruction> instructions;
    std::vector<std::pair<std::string, int>> branches;

    std::istringstream lines(source);
    std::string line;
    int lineNumber = 0;

    // This educational version expects each instruction to be on one line.
    // PTX directives and braces are skipped; labels may share a line with an instruction.
    while (std::getline(lines, line)) {
        ++lineNumber;
        std::string current = trim(line);
        if (current.empty() || current == "{" || current == "}") continue;

        std::smatch match;
        static const std::regex labelRegex(R"(^([A-Za-z_$][A-Za-z0-9_.$]*)\s*:\s*(.*)$)");
        if (std::regex_match(current, match, labelRegex)) {
            labels.insert(match[1].str());
            current = trim(match[2].str());
            if (current.empty()) continue;
        }

        // Ignore PTX directives such as .reg, .entry and .address_size.
        if (current.empty() || current[0] == '.') continue;

        std::size_t pos = 0;
        while (pos < current.size()) {
            const std::size_t semicolon = current.find(';', pos);
            if (semicolon == std::string::npos) break; // Ignore incomplete/non-instruction text.
            std::string statement = trim(current.substr(pos, semicolon - pos));
            pos = semicolon + 1;
            if (statement.empty() || statement == "{" || statement == "}") continue;

            const std::string opcode = opcodeFrom(statement);
            if (opcode.empty() || opcode[0] == '.') continue;
            instructions.push_back({opcode, statement, lineNumber});

            if (opcode == "bra" || opcode == "bra.uni") {
                std::istringstream words(statement);
                std::string first, target;
                words >> first;
                if (!first.empty() && first[0] == '@') words >> first;
                words >> target;
                while (!target.empty() && (target.back() == ',' || target.back() == ';'))
                    target.pop_back();
                if (!target.empty()) branches.push_back({target, lineNumber});
            }
        }
    }

    int arithmetic = 0, comparisons = 0, loads = 0, stores = 0, branchCount = 0;
    std::map<std::string, int> opcodeCounts;
    for (const auto& ins : instructions) {
        ++opcodeCounts[ins.opcode];
        if (startsWith(ins.opcode, "add") || startsWith(ins.opcode, "sub") ||
            startsWith(ins.opcode, "mul") || startsWith(ins.opcode, "mad") ||
            startsWith(ins.opcode, "div") || startsWith(ins.opcode, "min") ||
            startsWith(ins.opcode, "max")) ++arithmetic;
        if (startsWith(ins.opcode, "setp") || startsWith(ins.opcode, "set")) ++comparisons;
        if (startsWith(ins.opcode, "ld")) ++loads;
        if (startsWith(ins.opcode, "st")) ++stores;
        if (ins.opcode == "bra" || ins.opcode == "bra.uni") ++branchCount;
    }

    std::vector<std::string> errors;
    for (const auto& branch : branches) {
        if (!labels.count(branch.first)) {
            errors.push_back("Line " + std::to_string(branch.second) +
                ": branch target '" + branch.first + "' does not exist");
        }
    }

    std::cout << "========== PTX Inspector ==========\n";
    std::cout << "File                 : " << argv[1] << "\n";
    std::cout << "Total instructions   : " << instructions.size() << "\n";
    std::cout << "Arithmetic           : " << arithmetic << "\n";
    std::cout << "Comparisons          : " << comparisons << "\n";
    std::cout << "Memory loads         : " << loads << "\n";
    std::cout << "Memory stores        : " << stores << "\n";
    std::cout << "Branch instructions  : " << branchCount << "\n";
    std::cout << "\nInstruction summary:\n";
    for (const auto& entry : opcodeCounts) {
        std::cout << "  " << entry.first << " : " << entry.second << "\n";
    }

    if (errors.empty()) {
        std::cout << "\nBranch validation    : PASS\n";
    } else {
        std::cout << "\nBranch validation    : FAIL\n";
        for (const auto& error : errors) std::cout << "  ERROR: " << error << "\n";
    }
    std::cout << "===================================\n";
    return errors.empty() ? 0 : 1;
}
