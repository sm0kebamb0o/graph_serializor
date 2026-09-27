#include "cl_options.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace NGraphCompressor {
namespace {

std::string NormalizeShortName(std::string_view name) {
    if (name.size() == 1 && name[0] != '-') {
        return "-" + std::string(name);
    }
    if (name.size() == 2 && name[0] == '-' && name[1] != '-') {
        return std::string(name);
    }
    throw std::runtime_error("short option must have the form -x or x");
}

std::string NormalizeLongName(std::string_view name) {
    if (!name.empty() && name[0] != '-') {
        return "--" + std::string(name);
    }
    if (name.size() > 2 && name[0] == '-' && name[1] == '-') {
        return std::string(name);
    }
    throw std::runtime_error("long option must have the form --name or name");
}

} // anonymous namespace

TCLOptionsParser::TOption::TOption(std::optional<std::string> shortName,
                                   std::optional<std::string> longName,
                                   std::string_view description)
    : ShortName(std::move(shortName))
    , LongName(std::move(longName))
    , Description(description)
{
}

TCLOptionsParser::TOption& TCLOptionsParser::TOption::StoreTrue(bool& result) {
    Type = EOptionType::Flag;
    Callback = [&result](std::string_view) {
        result = true;
    };
    return *this;
}

TCLOptionsParser::TOption& TCLOptionsParser::TOption::Required(bool required) {
    IsRequired = required;
    return *this;
}

std::string TCLOptionsParser::TOption::GetDisplayName() const {
    if (ShortName && LongName) {
        return *ShortName + ", " + *LongName;
    }
    return ShortName ? *ShortName : *LongName;
}

TCLOptionsParser::TOption& TCLOptionsParser::AddOption(std::string_view shortName,
                                                       std::string_view longName,
                                                       std::string_view description)
{
    return AddOptionImpl(NormalizeShortName(shortName), NormalizeLongName(longName), description);
}

TCLOptionsParser::TOption& TCLOptionsParser::AddShortOption(std::string_view shortName,
                                                            std::string_view description)
{
    return AddOptionImpl(NormalizeShortName(shortName), std::nullopt, description);
}

TCLOptionsParser::TOption& TCLOptionsParser::AddLongOption(std::string_view longName,
                                                           std::string_view description)
{
    return AddOptionImpl(std::nullopt, NormalizeLongName(longName), description);
}

TCLOptionsParser::TOption& TCLOptionsParser::AddHelpOption() {
    TOption& option = AddOption("h", "help", "Print information about command-line options and exit");
    option.Type = EOptionType::Help;
    option.Callback = [this](std::string_view programName) {
        std::string help = "Usage: " + std::string(programName) + " [options]\nOptions:\n";
        for (const TOption& currentOption : Options_) {
            help += "  " + currentOption.GetDisplayName();
            if (currentOption.Type == EOptionType::Value) {
                help += " VALUE";
            }
            if (currentOption.IsRequired) {
                help += " (required)";
            }
            help += "\n      " + currentOption.Description + "\n";
        }
        std::cout << help << std::flush;
        if (!std::cout) {
            throw std::runtime_error("cannot write help message");
        }
        std::exit(EXIT_SUCCESS);
    };
    return option;
}

void TCLOptionsParser::Parse(int argc, char** argv) {
    const std::string_view programName = argc > 0 && argv[0] != nullptr ? argv[0] : "program";
    std::vector<bool> seenOptions(Options_.size(), false);

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        const std::size_t equalsPosition = argument.find('=');
        const std::string_view name = argument.substr(0, equalsPosition);
        const auto optionIt = OptionIndexes_.find(std::string(name));
        if (optionIt == OptionIndexes_.end()) {
            throw std::runtime_error("unknown command-line option: " + std::string(name));
        }
        const std::size_t optionIndex = optionIt->second;
        if (seenOptions[optionIndex]) {
            throw std::runtime_error("command-line option specified more than once: " + std::string(name));
        }
        seenOptions[optionIndex] = true;
        const TOption& option = Options_[optionIndex];

        if (option.Type == EOptionType::Flag || option.Type == EOptionType::Help) {
            if (equalsPosition != std::string_view::npos) {
                throw std::runtime_error("flag does not accept a value: " + std::string(name));
            }
            option.Callback(option.Type == EOptionType::Help ? programName : std::string_view{});
            continue;
        }

        std::string_view value;
        if (equalsPosition != std::string_view::npos) {
            value = argument.substr(equalsPosition + 1);
        } else {
            if (index + 1 >= argc) {
                throw std::runtime_error("option requires a value: " + std::string(name));
            }
            value = argv[++index];
        }
        if (value.empty()) {
            throw std::runtime_error("option value must not be empty: " + std::string(name));
        }
        option.Callback(value);
    }

    for (std::size_t index = 0; index < Options_.size(); ++index) {
        if (Options_[index].IsRequired && !seenOptions[index]) {
            throw std::runtime_error("required command-line option is missing: " + Options_[index].GetDisplayName());
        }
    }
}

TCLOptionsParser::TOption& TCLOptionsParser::AddOptionImpl(std::optional<std::string> shortName,
                                                           std::optional<std::string> longName,
                                                           std::string_view description)
{
    if ((shortName && OptionIndexes_.contains(*shortName)) ||
        (longName && OptionIndexes_.contains(*longName)))
    {
        throw std::runtime_error("duplicate command-line option");
    }

    const std::size_t optionIndex = Options_.size();
    // здесь ожидаю должен сработать move
    Options_.push_back(TOption(std::move(shortName), std::move(longName), description));
    if (Options_.back().ShortName) {
        OptionIndexes_.emplace(*Options_.back().ShortName, optionIndex);
    }
    if (Options_.back().LongName) {
        OptionIndexes_.emplace(*Options_.back().LongName, optionIndex);
    }
    return Options_.back();
}

} // namespace NGraphCompressor
