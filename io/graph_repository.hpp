#pragma once

#include "core/types.hpp"
#include "core/graph.hpp"
#include "io/diagnostics.hpp"
#include <string>
#include <memory>

namespace netopt {

class IGraphRepository {
public:
    virtual ~IGraphRepository() = default;

    virtual Result<Graph> load(const std::string& verticesPath,
                               const std::string& edgesPath,
                               DiagnosticsCollector& diagnostics) = 0;

    virtual Result<void> save(const Graph& graph,
                              const std::string& verticesPath,
                              const std::string& edgesPath) = 0;

    virtual Result<void> saveMstResult(const Graph& graph,
                                      const MstResult& result,
                                      const std::string& filepath) = 0;

    virtual Result<void> saveReport(const std::string& reportText,
                                   const std::string& filepath) = 0;
};

class CsvGraphRepository : public IGraphRepository {
public:
    Result<Graph> load(const std::string& verticesPath,
                       const std::string& edgesPath,
                       DiagnosticsCollector& diagnostics) override;

    Result<void> save(const Graph& graph,
                      const std::string& verticesPath,
                      const std::string& edgesPath) override;

    Result<void> saveMstResult(const Graph& graph,
                              const MstResult& result,
                              const std::string& filepath) override;

    Result<void> saveReport(const std::string& reportText,
                           const std::string& filepath) override;

private:
    static Result<std::string> readFile(const std::string& filepath);
    static Result<void> atomicWriteFile(const std::string& filepath, const std::string& content);
};

} // namespace netopt
