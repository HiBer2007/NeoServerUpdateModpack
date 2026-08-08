/**
 * @file nbt_binary_reader.cpp
 * @brief Implementation of NbtBinaryReader.
 */

#include "nbtcpp/nbt_binary_reader.h"
#include "nbtcpp/nbt_exception.h"

#include <algorithm>
#include <ios>
#include <istream>
#include <limits>

namespace nbtcpp {

// ─── Construction ─────────────────────────────────────────────────────────────

NbtBinaryReader::NbtBinaryReader(std::istream& stream, bool big_endian) noexcept
    : stream_(stream)
    , conv_(big_endian)
    , can_seek_(static_cast<bool>(stream.seekg(0, std::ios_base::cur)))
{
    // can_seek_ is best-effort; if tellg fails we treat as non-seekable.
    if (!can_seek_) stream_.clear();
}

// ─── Read primitives ─────────────────────────────────────────────────────────

NbtTagType NbtBinaryReader::read_tag_type() {
    int raw = stream_.get();
    if (raw == std::char_traits<char>::eof()) {
        throw NbtFormatException("Unexpected end of stream while reading tag type");
    }
    auto type = static_cast<NbtTagType>(static_cast<uint8_t>(raw));
    if (type > NbtTagType::LongArray && type != NbtTagType::Unknown) {
        throw NbtFormatException("NBT tag type out of range: " + std::to_string(raw));
    }
    return type;
}

uint8_t NbtBinaryReader::read_byte() {
    int raw = stream_.get();
    if (raw == std::char_traits<char>::eof()) {
        throw NbtFormatException("Unexpected end of stream while reading byte");
    }
    return static_cast<uint8_t>(raw);
}

int16_t NbtBinaryReader::read_int16() {
    fill_buffer(2);
    int16_t value;
    std::memcpy(&value, buffer_, sizeof(value));
    return conv_.convert(value);
}

int32_t NbtBinaryReader::read_int32() {
    fill_buffer(4);
    int32_t value;
    std::memcpy(&value, buffer_, sizeof(value));
    return conv_.convert(value);
}

int64_t NbtBinaryReader::read_int64() {
    fill_buffer(8);
    int64_t value;
    std::memcpy(&value, buffer_, sizeof(value));
    return conv_.convert(value);
}

float NbtBinaryReader::read_float() {
    fill_buffer(4);
    float value;
    std::memcpy(&value, buffer_, sizeof(value));
    return conv_.convert(value);
}

double NbtBinaryReader::read_double() {
    fill_buffer(8);
    double value;
    std::memcpy(&value, buffer_, sizeof(value));
    return conv_.convert(value);
}

// ─── Strings ─────────────────────────────────────────────────────────────────

std::string NbtBinaryReader::read_string() {
    int16_t byte_count = read_int16();
    if (byte_count < 0) {
        throw NbtFormatException("Negative string length: " + std::to_string(byte_count));
    }
    if (byte_count == 0) return {};

    std::string result(static_cast<size_t>(byte_count), '\0');
    read_raw(reinterpret_cast<uint8_t*>(result.data()), byte_count);
    return result;
}

void NbtBinaryReader::skip_string() {
    int16_t byte_count = read_int16();
    if (byte_count < 0) {
        throw NbtFormatException("Negative string length: " + std::to_string(byte_count));
    }
    if (byte_count > 0) skip(byte_count);
}

// ─── Skipping ────────────────────────────────────────────────────────────────

void NbtBinaryReader::skip(int64_t count) {
    if (count < 0) {
        throw std::invalid_argument("Cannot skip negative bytes");
    }
    if (count == 0) return;

    if (can_seek_) {
        stream_.seekg(count, std::ios_base::cur);
        if (stream_.fail()) {
            stream_.clear();
            throw NbtFormatException("Seek failed while skipping " + std::to_string(count) + " bytes");
        }
    } else {
        // Read and discard in chunks
        uint8_t buf[kSeekBufferSize];
        int64_t remaining = count;
        while (remaining > 0) {
            int64_t chunk = std::min(remaining, static_cast<int64_t>(kSeekBufferSize));
            auto got = static_cast<int64_t>(
                stream_.read(reinterpret_cast<char*>(buf), static_cast<std::streamsize>(chunk)).gcount());
            if (got == 0) {
                throw NbtFormatException("Unexpected end of stream while skipping");
            }
            remaining -= got;
        }
    }
}

std::vector<uint8_t> NbtBinaryReader::read_bytes(int32_t count) {
    if (count < 0) {
        throw std::invalid_argument("Cannot read negative bytes");
    }
    std::vector<uint8_t> result(static_cast<size_t>(count));
    auto actual = read_raw(result.data(), count);
    if (actual < count) {
        throw NbtFormatException("Unexpected end of stream: wanted " +
                                 std::to_string(count) + " bytes, got " + std::to_string(actual));
    }
    return result;
}

int32_t NbtBinaryReader::read_raw(uint8_t* buffer, int32_t count) {
    if (count == 0) return 0;
    auto got = static_cast<int32_t>(
        stream_.read(reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(count)).gcount());
    return got;
}

// ─── Internal helpers ────────────────────────────────────────────────────────

void NbtBinaryReader::fill_buffer(int num_bytes) {
    read_raw(buffer_, num_bytes);
    // gcount check is done inside read_raw / by the caller
    // but we also need to ensure we got exactly enough
    if (static_cast<int>(stream_.gcount()) < num_bytes) {
        throw NbtFormatException("Unexpected end of stream while reading " +
                                 std::to_string(num_bytes) + " bytes");
    }
}

} // namespace nbtcpp
