#pragma once

#include "graph.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>

namespace NGraphCompressor {

constexpr std::size_t BUFFER_SIZE = 1U << 20U;

std::optional<TEdge> ReadEdge(std::istream& input);

class TBitWriter {
public:
    explicit TBitWriter(std::ostream& output);
    ~TBitWriter();

    void WriteBit(bool bit);
    void WriteBits(std::uint64_t value, unsigned bitCount);
    void WriteGamma(std::uint64_t value);
    void WriteRice(std::uint64_t value, unsigned remainderBits);
    void Finish();

private:
    void FlushBuffer();

private:
    std::ostream& Output_;
    std::array<unsigned char, BUFFER_SIZE> Buffer_ = {};
    std::size_t Size_ = 0;
    unsigned BitCount_ = 0;
    unsigned char CurrentByte_ = 0;
};

class TBitReader {
public:
    explicit TBitReader(std::istream& input);

    bool ReadBit();
    std::uint64_t ReadBits(unsigned bitCount);
    std::uint64_t ReadGamma();
    std::uint64_t ReadRice(unsigned remainderBits);

private:
    unsigned char ReadByte();

private:
    std::istream& Input_;
    unsigned BitsLeft_ = 0;
    unsigned char CurrentByte_ = 0;
};

class TTextWriter {
public:
    explicit TTextWriter(std::ostream& output);
    ~TTextWriter();

    void WriteEdge(TId first, TId second, std::uint8_t weight);
    void Finish();

private:
    void WriteNumber(TId value);
    void WriteCharacter(char character);
    void Flush();

private:
    std::ostream& Output_;
    std::array<char, BUFFER_SIZE> Buffer_ = {};
    std::size_t Size_ = 0;
};

void WriteGroupCount(std::ostream& output, std::uint64_t groupCount);
std::uint64_t ReadGroupCount(std::istream& input);

} // namespace NGraphCompressor
