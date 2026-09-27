#pragma once

#include <charconv>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace NGraphCompressor {
namespace NDetail {

template <class T>
T ParseNumber(std::string_view value) {
    T result;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || end != value.data() + value.size()) {
        throw std::runtime_error("cannot parse command-line option value: " + std::string(value));
    }
    return result;
}

} // namespace NDetail

template <class T>
class StringConverter {
public:
    static T FromString(std::string_view s) = delete;
};

template <>
class StringConverter<std::string> {
public:
    static std::string FromString(std::string_view value) {
        return std::string(value);
    }
};

template <>
class StringConverter<int> {
public:
    static int FromString(std::string_view value) {
        return NDetail::ParseNumber<int>(value);
    }
};

} // namespace NGraphCompressor
