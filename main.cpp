#include "cl_options.h"
#include "graph.h"

#include <cstdio>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>

namespace NGraphCompressor {

struct TArguments {
    bool Serialize = false;
    bool Deserialize = false;
    std::string InputPath;
    std::string OutputPath;
};

TArguments ParseArguments(int argc, char** argv) {
    TArguments arguments;
    TCLOptionsParser parser;
    parser.AddHelpOption();
    parser.AddOption("s", "serialize", "Serialize a TSV graph into a binary file")
        .StoreTrue(arguments.Serialize);
    parser.AddOption("d", "deserialize", "Deserialize a binary graph into a TSV file")
        .StoreTrue(arguments.Deserialize);
    parser.AddOption("i", "input", "Input file path")
        .StoreResult(arguments.InputPath)
        .Required();
    parser.AddOption("o", "output", "Output file path")
        .StoreResult(arguments.OutputPath)
        .Required();
    parser.Parse(argc, argv);

    if (arguments.Serialize == arguments.Deserialize) {
        throw std::runtime_error("exactly one of -s/--serialize and -d/--deserialize must be specified");
    }
    return arguments;
}

TGraph ReadGraph(const TArguments& arguments) {
    std::ifstream input(arguments.InputPath, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open '" + arguments.InputPath + "' for reading");
    }

    auto graph = arguments.Serialize
        ? TGraph::Read(input)
        : TGraph::Deserialize(input);
    input.close();
    return graph;
}

void WriteGraph(const TGraph& graph, const TArguments& arguments) {
    std::ofstream output(arguments.OutputPath, std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot open '" + arguments.OutputPath + "' for writing");
    }

    if (arguments.Serialize) {
        graph.Serialize(output);
    } else {
        graph.Write(output);
    }
    output.close();
}

} // namespace NGraphCompressor

int main(int argc, char** argv) {
    try {
        const NGraphCompressor::TArguments arguments = NGraphCompressor::ParseArguments(argc, argv);
        NGraphCompressor::TGraph graph = NGraphCompressor::ReadGraph(arguments);
        NGraphCompressor::WriteGraph(graph, arguments);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "error: %s\n", error.what());
        return 1;
    }
}
