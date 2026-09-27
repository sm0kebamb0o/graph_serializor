#include "graph.h"
#include "graph_io.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace NGraphCompressor {
namespace {

// по условию задачи максиамльно возможный id есть 2^32 - 1
constexpr std::uint64_t VERTEX_ID_RANGE =
    std::uint64_t{1} << std::numeric_limits<TId>::digits;

// по условию задачи максиамльно возможный вес есть 2^8 - 1
constexpr unsigned WEIGHT_BITS = std::numeric_limits<decltype(TEdge::Weight)>::digits;

unsigned GetRiceBits(std::uint64_t range, std::uint64_t count) {
    if (count == 0 || range <= count) {
        return 0;
    }

    const std::uint64_t meanGap = range / count;
    return std::bit_width(meanGap) - 1U;
}

std::uint64_t GetNextBase(std::uint64_t value) {
    return value + 1U;
}

template <class T>
T TryReadBitsValue(std::uint64_t value, std::string_view errorMessage) {
    if (value > std::numeric_limits<T>::max()) {
        throw std::runtime_error(std::string(errorMessage));
    }
    return static_cast<T>(value);
}

} // anonymous namespace

TGraph TGraph::Read(std::istream& input) {
    TGraph graph;
    std::vector<TEdge> edges;

    while (std::optional<TEdge> edge = ReadEdge(input)) {
        if (edge->BeginVertexId > edge->EndVertexId) {
            std::swap(edge->BeginVertexId, edge->EndVertexId);
        }
        edges.push_back(std::move(*edge));
    }
    std::sort(edges.begin(), edges.end(), [](const TEdge& left, const TEdge& right) {
        if (left.BeginVertexId != right.BeginVertexId) {
            return left.BeginVertexId < right.BeginVertexId;
        }
        return left.EndVertexId < right.EndVertexId;
    });
    graph.Edges_.reserve(edges.size());
    for (TId edgeId = 0; edgeId < edges.size(); ++edgeId) {
        edges[edgeId].Id = edgeId;
        graph.Edges_.emplace(edgeId, edges[edgeId]);
        graph.VertexIdToEdgeIds_[edges[edgeId].BeginVertexId].push_back(edgeId);
    }

    return graph;
}

void TGraph::Serialize(std::ostream& output) const {
    WriteGroupCount(output, VertexIdToEdgeIds_.size());
    TBitWriter writer(output);

    const unsigned groupRiceBits = GetRiceBits(
        VERTEX_ID_RANGE, VertexIdToEdgeIds_.size());
    std::uint64_t groupBase = 0;
    for (const auto& [vertexId, edgeIds] : VertexIdToEdgeIds_) {
        const std::size_t degree = edgeIds.size();
        writer.WriteRice(vertexId - groupBase, groupRiceBits);
        writer.WriteGamma(degree);

        const unsigned neighborRiceBits = GetRiceBits(
            VERTEX_ID_RANGE - vertexId, degree);
        std::uint64_t neighborBase = vertexId;
        for (TId edgeId : edgeIds) {
            const TEdge& edge = Edges_.at(edgeId);
            writer.WriteRice(edge.EndVertexId - neighborBase, neighborRiceBits);
            writer.WriteBits(edge.Weight, WEIGHT_BITS);
            neighborBase = GetNextBase(edge.EndVertexId);
        }
        groupBase = GetNextBase(vertexId);
    }
    writer.Finish();
}

TGraph TGraph::Deserialize(std::istream& input) {
    const std::uint64_t groupCount = ReadGroupCount(input);
    if (groupCount > std::numeric_limits<TId>::max()) {
        throw std::runtime_error("binary graph is too large");
    }
    TBitReader reader(input);

    TGraph graph;
    const unsigned groupRiceBits = GetRiceBits(
        VERTEX_ID_RANGE, groupCount);
    std::uint64_t groupBase = 0;
    TId edgeId = 0;
    for (std::uint64_t groupIndex = 0; groupIndex < groupCount; ++groupIndex) {
        const TId vertexId = TryReadBitsValue<TId>(
            groupBase + reader.ReadRice(groupRiceBits),
            "invalid vertex identifier in the binary file");

        const std::uint32_t degree = TryReadBitsValue<std::uint32_t>(
            reader.ReadGamma(), "invalid vertex degree in the binary file");
        std::vector<TId>& edgeIds = graph.VertexIdToEdgeIds_[vertexId];
        edgeIds.reserve(degree);

        const unsigned neighborRiceBits = GetRiceBits(
            VERTEX_ID_RANGE - vertexId, degree);
        std::uint64_t neighborBase = vertexId;
        for (std::uint64_t index = 0; index < degree; ++index) {
            const TId neighborId = TryReadBitsValue<TId>(
                neighborBase + reader.ReadRice(neighborRiceBits),
                "invalid neighbor identifier in the binary file");
            const auto weight = TryReadBitsValue<decltype(TEdge::Weight)>(
                reader.ReadBits(WEIGHT_BITS), "invalid edge weight in the binary file");
            graph.Edges_.emplace(edgeId,
                                 TEdge{edgeId, vertexId, neighborId, weight});
            edgeIds.push_back(edgeId++);
            neighborBase = GetNextBase(neighborId);
        }
        groupBase = GetNextBase(vertexId);
    }

    return graph;
}

void TGraph::Write(std::ostream& output) const {
    TTextWriter writer(output);
    for (TId edgeId = 0; edgeId < Edges_.size(); ++edgeId) {
        const TEdge& edge = Edges_.at(edgeId);
        writer.WriteEdge(edge.BeginVertexId, edge.EndVertexId, edge.Weight);
    }
    writer.Finish();
}

} // namespace NGraphCompressor
