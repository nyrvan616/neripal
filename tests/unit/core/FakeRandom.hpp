#pragma once

#include "neripal/core/IRandom.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

class FakeRandom final : public neripal::core::IRandom {
public:
    FakeRandom() = default;
    explicit FakeRandom(std::vector<std::uint32_t> values) : values_(std::move(values)) {}

    void push(std::uint32_t value) { values_.push_back(value); }

    std::uint32_t nextU32() override {
        if (index_ >= values_.size()) {
            throw std::logic_error("FakeRandom exhausted");
        }
        return values_[index_++];
    }

    std::size_t remaining() const noexcept { return values_.size() - index_; }

private:
    std::vector<std::uint32_t> values_{};
    std::size_t index_ = 0;
};
