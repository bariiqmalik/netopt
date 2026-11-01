#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iostream>

namespace netopt {

enum class DiagnosticSeverity {
    Info,
    Warning,
    Error
};

struct Diagnostic {
    DiagnosticSeverity severity{DiagnosticSeverity::Info};
    std::string filename;
    size_t lineNumber{0};
    std::string message;

    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        if (!filename.empty()) {
            oss << filename << ":";
        }
        if (lineNumber > 0) {
            oss << lineNumber << ": ";
        } else if (!filename.empty()) {
            oss << " ";
        }

        switch (severity) {
            case DiagnosticSeverity::Info:
                oss << "[INFO] ";
                break;
            case DiagnosticSeverity::Warning:
                oss << "[WARNING] ";
                break;
            case DiagnosticSeverity::Error:
                oss << "[ERROR] ";
                break;
        }
        oss << message;
        return oss.str();
    }
};

class DiagnosticsCollector {
public:
    void add(DiagnosticSeverity severity, std::string filename, size_t lineNumber, std::string message) {
        diagnostics_.push_back(Diagnostic{
            severity,
            std::move(filename),
            lineNumber,
            std::move(message)
        });
    }

    void addInfo(std::string filename, size_t lineNumber, std::string message) {
        add(DiagnosticSeverity::Info, std::move(filename), lineNumber, std::move(message));
    }

    void addWarning(std::string filename, size_t lineNumber, std::string message) {
        add(DiagnosticSeverity::Warning, std::move(filename), lineNumber, std::move(message));
    }

    void addError(std::string filename, size_t lineNumber, std::string message) {
        add(DiagnosticSeverity::Error, std::move(filename), lineNumber, std::move(message));
    }

    [[nodiscard]] bool hasErrors() const noexcept {
        for (const auto& d : diagnostics_) {
            if (d.severity == DiagnosticSeverity::Error) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool hasWarnings() const noexcept {
        for (const auto& d : diagnostics_) {
            if (d.severity == DiagnosticSeverity::Warning) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept {
        return diagnostics_;
    }

    [[nodiscard]] std::string formatAll() const {
        std::ostringstream oss;
        for (const auto& d : diagnostics_) {
            oss << d.toString() << "\n";
        }
        return oss.str();
    }

    void clear() noexcept {
        diagnostics_.clear();
    }

private:
    std::vector<Diagnostic> diagnostics_;
};

} // namespace netopt
