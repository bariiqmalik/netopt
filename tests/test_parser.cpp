#include "doctest.h"
#include "io/csv_tokenizer.hpp"
#include "io/row_validator.hpp"
#include "io/graph_repository.hpp"
#include "core/graph_builder.hpp"
#include <filesystem>
#include <fstream>

using namespace netopt;

TEST_CASE("CSV Tokenizer edge cases: BOM, CRLF, trailing blanks, quotes") {
    DiagnosticsCollector diag;

    // UTF-8 BOM + CRLF + Quoted fields with commas + trailing blanks
    std::string csvData = "\xEF\xBB\xBF#netopt-schema:1\r\n"
                          "id,label,type,x,y   \r\n"
                          " 1 , \"Room 101, Main\" , server , 1.5 , 2.5 \r\n"
                          "2, \"Node \"\"B\"\"\", router, 3.0, 4.0\r\n";

    auto res = CsvTokenizer::tokenize(csvData, "test.csv", diag);
    REQUIRE(res.isOk());
    const auto& rows = res.value();
    REQUIRE(rows.size() == 4); // 1 comment, 1 header, 2 data rows

    CHECK(rows[0].isComment);
    CHECK(rows[0].commentText == "#netopt-schema:1");

    CHECK(rows[1].fields[0] == "id");
    CHECK(rows[1].fields[4] == "y");

    CHECK(rows[2].fields[0] == "1");
    CHECK(rows[2].fields[1] == "Room 101, Main");
    CHECK(rows[2].fields[2] == "server");
    CHECK(rows[2].fields[3] == "1.5");
    CHECK(rows[2].fields[4] == "2.5");

    CHECK(rows[3].fields[0] == "2");
    CHECK(rows[3].fields[1] == "Node \"B\"");
}

TEST_CASE("Edge-case table compliance tests") {
    DiagnosticsCollector diag;

    SUBCASE("Case: Empty file / header only -> Error 'no vertices'") {
        diag.clear();
        std::string vCsv = "#netopt-schema:1\nid,label,type,x,y\n";
        auto rows = CsvTokenizer::tokenize(vCsv, "v.csv", diag).value();
        GraphBuilder builder;
        auto res = RowValidator::validateAndLoadVertices(rows, builder, "v.csv", diag);
        CHECK(res.isErr());
        CHECK(diag.hasErrors());
    }

    SUBCASE("Case: Missing or extra columns -> Error with line number") {
        diag.clear();
        std::string vCsv = "#netopt-schema:1\nid,label,type,x,y\n1,V1,server,0.0\n"; // 4 cols instead of 5
        auto rows = CsvTokenizer::tokenize(vCsv, "v.csv", diag).value();
        GraphBuilder builder;
        RowValidator::validateAndLoadVertices(rows, builder, "v.csv", diag);
        CHECK(diag.hasErrors());
        CHECK(diag.diagnostics()[0].lineNumber == 3);
    }

    SUBCASE("Case: Non-numeric / NaN / overflow cost -> Error") {
        diag.clear();
        std::string eCsv = "#netopt-schema:1\nsource,target,cost,length_m,medium\n1,2,NotANumber,10.0,cat6\n";
        auto rows = CsvTokenizer::tokenize(eCsv, "e.csv", diag).value();
        GraphBuilder builder;
        RowValidator::validateAndLoadEdges(rows, builder, "e.csv", diag);
        CHECK(diag.hasErrors());
    }

    SUBCASE("Case: Negative cost -> Error") {
        diag.clear();
        GraphBuilder builder;
        builder.addVertex(1, "A");
        builder.addVertex(2, "B");
        builder.addEdge(1, 2, -500, 10.0, "cat6", 4);
        auto gRes = builder.build(diag, "edges.csv");
        CHECK(gRes.isErr());
        CHECK(diag.hasErrors());
        CHECK(diag.diagnostics()[0].lineNumber == 4);
    }

    SUBCASE("Case: Zero cost -> Warning, accepted") {
        diag.clear();
        GraphBuilder builder;
        builder.addVertex(1, "A");
        builder.addVertex(2, "B");
        builder.addEdge(1, 2, 0, 10.0, "cat6", 5);
        auto gRes = builder.build(diag, "edges.csv");
        CHECK(gRes.isOk());
        CHECK(diag.hasWarnings());
        CHECK_FALSE(diag.hasErrors());
        CHECK(gRes.value().edge(0).cost == 0);
    }

    SUBCASE("Case: Duplicate vertex id -> Error") {
        diag.clear();
        std::string vCsv = "#netopt-schema:1\nid,label,type,x,y\n1,A,host,0,0\n1,DuplicateA,host,1,1\n";
        auto rows = CsvTokenizer::tokenize(vCsv, "v.csv", diag).value();
        GraphBuilder builder;
        RowValidator::validateAndLoadVertices(rows, builder, "v.csv", diag);
        CHECK(diag.hasErrors());
        CHECK(diag.diagnostics()[0].lineNumber == 4);
    }

    SUBCASE("Case: Edge references unknown vertex -> Error") {
        diag.clear();
        GraphBuilder builder;
        builder.addVertex(1, "A");
        builder.addEdge(1, 999, 100, 1.0, "cat6", 10);
        auto gRes = builder.build(diag, "e.csv");
        CHECK(gRes.isErr());
        CHECK(diag.hasErrors());
    }

    SUBCASE("Case: Self-loop -> Warning, dropped") {
        diag.clear();
        GraphBuilder builder;
        builder.addVertex(1, "A");
        builder.addEdge(1, 1, 100, 1.0, "cat6", 12);
        auto gRes = builder.build(diag, "e.csv");
        CHECK(gRes.isOk());
        CHECK(diag.hasWarnings());
        CHECK(gRes.value().edgeCount() == 0);
    }

    SUBCASE("Case: Parallel / reversed edges -> Warning, keep cheapest") {
        diag.clear();
        GraphBuilder builder;
        builder.addVertex(1, "A");
        builder.addVertex(2, "B");
        builder.addEdge(1, 2, 500, 1.0, "cat6", 2);
        builder.addEdge(2, 1, 300, 1.0, "fiber", 3); // reversed and cheaper!
        auto gRes = builder.build(diag, "e.csv");
        CHECK(gRes.isOk());
        CHECK(diag.hasWarnings());
        CHECK(gRes.value().edgeCount() == 1);
        CHECK(gRes.value().edge(0).cost == 300);
    }

    SUBCASE("Case: Single vertex, no edges -> Valid, cost 0") {
        diag.clear();
        GraphBuilder builder;
        builder.addVertex(1, "SoloNode");
        auto gRes = builder.build(diag, "single.csv");
        CHECK(gRes.isOk());
        CHECK_FALSE(diag.hasErrors());
        CHECK(gRes.value().vertexCount() == 1);
        CHECK(gRes.value().edgeCount() == 0);
    }

    SUBCASE("Case: Missing file / unreadable path -> Error with clear message") {
        diag.clear();
        CsvGraphRepository repo;
        auto res = repo.load("non_existent_v.csv", "non_existent_e.csv", diag);
        CHECK(res.isErr());
        CHECK(diag.hasErrors());
    }

    SUBCASE("Case: Atomic save creates valid file without corruption") {
        CsvGraphRepository repo;
        GraphBuilder builder;
        builder.addVertex(100, "N1");
        builder.addVertex(200, "N2");
        builder.addEdge(100, 200, 450, 12.5, "cat6");
        auto g = builder.build(diag).value();

        const std::string vFile = "test_atomic_v.csv";
        const std::string eFile = "test_atomic_e.csv";

        auto saveRes = repo.save(g, vFile, eFile);
        CHECK(saveRes.isOk());
        CHECK(std::filesystem::exists(vFile));
        CHECK(std::filesystem::exists(eFile));
        CHECK_FALSE(std::filesystem::exists(vFile + ".tmp"));
        CHECK_FALSE(std::filesystem::exists(eFile + ".tmp"));

        // Clean up test files
        std::filesystem::remove(vFile);
        std::filesystem::remove(eFile);
    }
}
