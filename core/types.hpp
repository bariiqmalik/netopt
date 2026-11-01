#pragma once

#include <cstdint>
#include <concepts>
#include <string>
#include <vector>
#include <optional>
#include <utility>
#include <stdexcept>

namespace netopt {

// Core type definitions
using VertexId = uint32_t;
using EdgeId = uint32_t;
using Cost = int64_t;

// C++20 Concept for cost/weight constraint (integral minor currency units)
template <typename T>
concept NumericCost = std::integral<T> && !std::same_as<T, bool>;

// Result type for standard C++20 error handling without std::expected or exceptions
template <typename T, typename E = std::string>
class Result {
public:
    Result(const T& val) : val_(val) {}
    Result(T&& val) : val_(std::move(val)) {}
    Result(const E& err, bool is_error_tag) : err_(err) { (void)is_error_tag; }
    Result(E&& err, bool is_error_tag) : err_(std::move(err)) { (void)is_error_tag; }

    static Result<T, E> ok(T val) {
        Result<T, E> res;
        res.val_ = std::move(val);
        return res;
    }

    static Result<T, E> err(E err_val) {
        Result<T, E> res;
        res.err_ = std::move(err_val);
        return res;
    }

    [[nodiscard]] bool isOk() const noexcept {
        return val_.has_value();
    }

    [[nodiscard]] bool isErr() const noexcept {
        return err_.has_value();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return isOk();
    }

    [[nodiscard]] const T& value() const {
        if (!val_.has_value()) {
            throw std::runtime_error("Accessed value on error Result");
        }
        return *val_;
    }

    [[nodiscard]] T& value() {
        if (!val_.has_value()) {
            throw std::runtime_error("Accessed value on error Result");
        }
        return *val_;
    }

    [[nodiscard]] const E& error() const {
        if (!err_.has_value()) {
            throw std::runtime_error("Accessed error on ok Result");
        }
        return *err_;
    }

    [[nodiscard]] E& error() {
        if (!err_.has_value()) {
            throw std::runtime_error("Accessed error on ok Result");
        }
        return *err_;
    }

private:
    Result() = default;
    std::optional<T> val_;
    std::optional<E> err_;
};

// Specialization of Result for void
template <typename E>
class Result<void, E> {
public:
    Result() : err_(std::nullopt) {}
    explicit Result(E err_val) : err_(std::move(err_val)) {}

    static Result<void, E> ok() {
        return Result();
    }

    static Result<void, E> err(E err_val) {
        return Result(std::move(err_val));
    }

    [[nodiscard]] bool isOk() const noexcept {
        return !err_.has_value();
    }

    [[nodiscard]] bool isErr() const noexcept {
        return err_.has_value();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return isOk();
    }

    [[nodiscard]] const E& error() const {
        if (!err_.has_value()) {
            throw std::runtime_error("Accessed error on ok Result<void>");
        }
        return *err_;
    }

private:
    std::optional<E> err_;
};

// Edge representation with strict total order by (cost, id)
struct Edge {
    VertexId u{0};
    VertexId v{0};
    Cost cost{0};
    EdgeId id{0};

    [[nodiscard]] constexpr auto operator<=>(const Edge& other) const noexcept {
        if (cost != other.cost) {
            return cost <=> other.cost;
        }
        return id <=> other.id;
    }

    [[nodiscard]] constexpr bool operator==(const Edge& other) const noexcept {
        return cost == other.cost && id == other.id && u == other.u && v == other.v;
    }
};

// Arc for Compressed Sparse Row (CSR) adjacency list
struct Arc {
    VertexId target{0};
    Cost cost{0};
    EdgeId id{0};
};

// Vertex metadata
struct Vertex {
    int64_t externalId{0};
    std::string label;
    std::string type;
    double x{0.0};
    double y{0.0};
};

// Edge metadata
struct EdgeMetadata {
    double length_m{0.0};
    std::string medium;
};

// Minimum Spanning Tree / Forest Result
struct MstResult {
    std::vector<EdgeId> edges;
    Cost totalCost{0};
    size_t components{0};

    [[nodiscard]] bool connected() const noexcept {
        return components <= 1;
    }
};

} // namespace netopt
