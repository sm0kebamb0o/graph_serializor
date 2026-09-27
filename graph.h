#pragma once

#include <cstdint>
#include <iosfwd>
#include <map>
#include <unordered_map>
#include <vector>

namespace NGraphCompressor {

using TId = std::uint32_t;

struct TEdge {
    TId Id;
    TId BeginVertexId;
    TId EndVertexId;
    std::uint8_t Weight;
};

class TGraph {
public:
    static TGraph Read(std::istream& input);
    static TGraph Deserialize(std::istream& input);

    void Write(std::ostream& output) const;
    void Serialize(std::ostream& output) const;

private:
    TGraph() = default;

private:
    std::unordered_map<TId, TEdge> Edges_;
    std::map<TId, std::vector<TId>> VertexIdToEdgeIds_;
};

} // namespace NGraphCompressor
