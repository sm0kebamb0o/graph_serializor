#include "graph_io.h"

#include <array>
#include <bit>
#include <charconv>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace NGraphCompressor {
namespace {

std::uint64_t ParseUnsigned(std::string_view value) {
    std::uint64_t result = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || end != value.data() + value.size()) {
        throw std::runtime_error("expected a non-negative integer in the input file");
    }
    return result;
}

class LittleEndianParser {
public:
    template <class T>
    static void Write(std::ostream& output, T value) {
        for (std::size_t index = 0; index < sizeof(T); ++index) {
            output.put(static_cast<char>(
                value >> (index * std::numeric_limits<unsigned char>::digits)));
        }
        if (!output) {
            throw std::runtime_error("cannot write the binary header");
        }
    }

    template <class T>
    static T Read(std::istream& input) {
        T result = 0;
        for (std::size_t index = 0; index < sizeof(T); ++index) {
            const std::istream::int_type byte = input.get();
            if (byte == std::istream::traits_type::eof()) {
                throw std::runtime_error("cannot read the binary header");
            }
            result |= static_cast<T>(static_cast<unsigned char>(byte))
                << (index * std::numeric_limits<unsigned char>::digits);
        }
        return result;
    }
};

} // anonymous namespace

std::optional<TEdge> ReadEdge(std::istream& input) {
    std::string line;
    if (!std::getline(input, line)) {
        if (!input.eof()) {
            throw std::runtime_error("cannot read the input file");
        }
        return std::nullopt;
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    const std::size_t firstTab = line.find('\t');
    const std::size_t secondTab = firstTab == std::string::npos
        ? std::string::npos
        : line.find('\t', firstTab + 1);
    if (firstTab == std::string::npos ||
        secondTab == std::string::npos ||
        line.find('\t', secondTab + 1) != std::string::npos)
    {
        throw std::runtime_error("an input edge must contain exactly three tab-separated values");
    }

    const std::string_view lineView(line);
    const std::uint64_t firstValue = ParseUnsigned(lineView.substr(0, firstTab));
    const std::uint64_t secondValue = ParseUnsigned(
        lineView.substr(firstTab + 1, secondTab - firstTab - 1));
    const std::uint64_t weightValue = ParseUnsigned(lineView.substr(secondTab + 1));
    if (firstValue > std::numeric_limits<TId>::max() ||
        secondValue > std::numeric_limits<TId>::max())
    {
        throw std::runtime_error("a vertex identifier is outside the allowed range");
    }
    if (weightValue > std::numeric_limits<decltype(TEdge::Weight)>::max()) {
        throw std::runtime_error("the edge weight is outside the allowed range");
    }

    return TEdge{
        0,
        static_cast<TId>(firstValue),
        static_cast<TId>(secondValue),
        static_cast<std::uint8_t>(weightValue)
    };
}

TBitWriter::TBitWriter(std::ostream& output)
    : Output_(output)
{
}

TBitWriter::~TBitWriter() {
    try {
        Finish();
    } catch (...) {
    }
}

void TBitWriter::WriteBit(bool bit) {
    if (bit) {
        CurrentByte_ |= static_cast<unsigned char>(
            1U << (std::numeric_limits<unsigned char>::digits - 1U - BitCount_));
    }
    ++BitCount_;
    if (BitCount_ == std::numeric_limits<unsigned char>::digits) {
        Buffer_[Size_++] = CurrentByte_;
        CurrentByte_ = 0;
        BitCount_ = 0;
        if (Size_ == Buffer_.size()) {
            FlushBuffer();
        }
    }
}

void TBitWriter::WriteBits(std::uint64_t value, unsigned bitCount) {
    for (unsigned index = bitCount; index > 0; --index) {
        WriteBit(((value >> (index - 1U)) & 1U) != 0);
    }
}

void TBitWriter::WriteGamma(std::uint64_t value) {
    if (value == 0) {
        throw std::runtime_error("gamma code cannot represent zero");
    }
    const unsigned highestBit = std::bit_width(value) - 1;
    for (unsigned index = 0; index < highestBit; ++index) {
        WriteBit(false);
    }
    WriteBits(value, highestBit + 1U);
}

void TBitWriter::WriteRice(std::uint64_t value, unsigned remainderBits) {
    const std::uint64_t quotient = remainderBits == std::numeric_limits<std::uint64_t>::digits
        ? 0
        : value >> remainderBits;
    for (std::uint64_t index = 0; index < quotient; ++index) {
        WriteBit(false);
    }
    WriteBit(true);
    if (remainderBits != 0) {
        WriteBits(value, remainderBits);
    }
}

void TBitWriter::Finish() {
    if (BitCount_ != 0) {
        Buffer_[Size_++] = CurrentByte_;
        BitCount_ = 0;
        CurrentByte_ = 0;
    }
    FlushBuffer();
}

void TBitWriter::FlushBuffer() {
    if (Size_ != 0) {
        Output_.write(reinterpret_cast<const char*>(Buffer_.data()), Size_);
        if (!Output_) {
            throw std::runtime_error("cannot write the binary output file");
        }
    }
    Size_ = 0;
}

TBitReader::TBitReader(std::istream& input)
    : Input_(input)
{
}

bool TBitReader::ReadBit() {
    if (BitsLeft_ == 0) {
        CurrentByte_ = ReadByte();
        BitsLeft_ = std::numeric_limits<unsigned char>::digits;
    }
    --BitsLeft_;
    return ((CurrentByte_ >> BitsLeft_) & 1U) != 0;
}

std::uint64_t TBitReader::ReadBits(unsigned bitCount) {
    std::uint64_t result = 0;
    for (unsigned index = 0; index < bitCount; ++index) {
        result = (result << 1U) | static_cast<unsigned>(ReadBit());
    }
    return result;
}

std::uint64_t TBitReader::ReadGamma() {
    unsigned leadingZeros = 0;
    while (!ReadBit()) {
        ++leadingZeros;
        if (leadingZeros >= std::numeric_limits<std::uint64_t>::digits) {
            throw std::runtime_error("invalid gamma code in the binary file");
        }
    }
    return (std::uint64_t{1} << leadingZeros) | ReadBits(leadingZeros);
}

std::uint64_t TBitReader::ReadRice(unsigned remainderBits) {
    std::uint64_t quotient = 0;
    while (!ReadBit()) {
        ++quotient;
        if (quotient > std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("invalid Rice code in the binary file");
        }
    }
    return (quotient << remainderBits) | ReadBits(remainderBits);
}

unsigned char TBitReader::ReadByte() {
    const int value = Input_.get();
    if (value == std::char_traits<char>::eof()) {
        if (Input_.bad()) {
            throw std::runtime_error("cannot read the binary input file");
        }
        throw std::runtime_error("unexpected end of the binary input file");
    }
    return static_cast<unsigned char>(value);
}

TTextWriter::TTextWriter(std::ostream& output)
    : Output_(output)
{
}

TTextWriter::~TTextWriter() {
    try {
        Finish();
    } catch (...) {
    }
}

void TTextWriter::WriteEdge(TId first, TId second, std::uint8_t weight) {
    WriteNumber(first);
    WriteCharacter('\t');
    WriteNumber(second);
    WriteCharacter('\t');
    WriteNumber(weight);
    WriteCharacter('\n');
}

void TTextWriter::Finish() {
    Flush();
}

void TTextWriter::WriteNumber(TId value) {
    std::array<char, std::numeric_limits<TId>::digits10 + 1U> digits = {};
    std::size_t size = 0;
    do {
        digits[size++] = static_cast<char>('0' + value % 10U);
        value /= 10U;
    } while (value != 0);
    while (size != 0) {
        WriteCharacter(digits[--size]);
    }
}

void TTextWriter::WriteCharacter(char character) {
    Buffer_[Size_++] = character;
    if (Size_ == Buffer_.size()) {
        Flush();
    }
}

void TTextWriter::Flush() {
    if (Size_ != 0) {
        Output_.write(Buffer_.data(), Size_);
        if (!Output_) {
            throw std::runtime_error("cannot write the text output file");
        }
    }
    Size_ = 0;
}

void WriteGroupCount(std::ostream& output, std::uint64_t groupCount) {
    LittleEndianParser::Write(output, groupCount);
}

std::uint64_t ReadGroupCount(std::istream& input) {
    return LittleEndianParser::Read<std::uint64_t>(input);
}

} // namespace NGraphCompressor
