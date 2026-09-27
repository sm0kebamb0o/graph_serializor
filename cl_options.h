#pragma once

#include "string_converter.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace NGraphCompressor {

class TCLOptionsParser {
private:
    enum class EOptionType {
        Unspecified,
        Flag,
        Help,
        Value
    };

public:
    class TOption {
    public:
        TOption& StoreTrue(bool& result);

        template <class T>
        TOption& StoreResult(T& result) {
            Type = EOptionType::Value;
            Callback = [&result](std::string_view value) {
                // выглядит конечно супер костыльно с отдельным парсером
                result = StringConverter<T>::FromString(value);
            };
            return *this;
        }

        TOption& Required(bool required = true);
        std::string GetDisplayName() const;

    private:
        friend class TCLOptionsParser;

        TOption(std::optional<std::string> shortName,
                std::optional<std::string> longName,
                std::string_view description);

    private:
        std::optional<std::string> ShortName;
        std::optional<std::string> LongName;
        std::string Description;
        EOptionType Type = EOptionType::Unspecified;
        std::function<void(std::string_view)> Callback;
        bool IsRequired = false;
    };

public:
    TOption& AddOption(std::string_view shortName,
                       std::string_view longName,
                       std::string_view description);

    TOption& AddShortOption(std::string_view shortName,
                            std::string_view description);

    TOption& AddLongOption(std::string_view longName,
                           std::string_view description);

    TOption& AddHelpOption();

    void Parse(int argc, char** argv);

private:
    TOption& AddOptionImpl(std::optional<std::string> shortName,
                           std::optional<std::string> longName,
                           std::string_view description);

private:
    std::vector<TOption> Options_;
    std::unordered_map<std::string, std::size_t> OptionIndexes_;
};

} // namespace NGraphCompressor
