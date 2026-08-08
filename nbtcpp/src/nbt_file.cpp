/**
 * @file nbt_file.cpp
 * @brief Implementation of NbtFile.
 *
 * Uses zlib for GZip and ZLib compression/decompression.
 */

#include "nbtcpp/nbt_file.h"
#include "nbtcpp/nbt_binary_reader.h"
#include "nbtcpp/nbt_binary_writer.h"
#include "nbtcpp/nbt_exception.h"

#include <zlib.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace nbtcpp {

// ─── Static initializers ─────────────────────────────────────────────────────

int NbtFile::s_default_buffer_size = 8192;
bool NbtFile::s_big_endian_by_default = true;

// ─── Constructors ────────────────────────────────────────────────────────────

NbtFile::NbtFile()
    : root_tag_(std::make_shared<NbtCompound>(""))
    , big_endian_(s_big_endian_by_default)
    , buffer_size_(s_default_buffer_size)
{
}

NbtFile::NbtFile(std::shared_ptr<NbtCompound> root_tag)
    : NbtFile()
{
    set_root_tag(std::move(root_tag));
}

NbtFile::NbtFile(const std::string& filepath)
    : NbtFile()
{
    load_from_file(filepath);
}

// ─── Properties ──────────────────────────────────────────────────────────────

void NbtFile::set_root_tag(std::shared_ptr<NbtCompound> tag) {
    if (!tag) throw std::invalid_argument("Root tag must not be null");
    if (tag->name().empty()) throw std::invalid_argument("Root tag must be named");
    root_tag_ = std::move(tag);
}

void NbtFile::set_buffer_size(int size) {
    if (size < 0) throw std::invalid_argument("Buffer size cannot be negative");
    buffer_size_ = size;
}

void NbtFile::set_default_buffer_size(int size) {
    if (size < 0) throw std::invalid_argument("Default buffer size cannot be negative");
    s_default_buffer_size = size;
}

// ─── Compression detection ───────────────────────────────────────────────────

NbtCompression NbtFile::detect_compression(std::istream& stream) {
    if (!stream) throw std::invalid_argument("Stream is not readable");
    auto pos = stream.tellg();
    if (pos < 0) {
        throw std::runtime_error("Cannot auto-detect compression on non-seekable stream");
    }

    int first = stream.get();
    if (first == std::char_traits<char>::eof()) {
        throw std::runtime_error("Unexpected end of stream while detecting compression");
    }

    NbtCompression result;
    switch (first) {
        // NBT tag type IDs (0x01-0x0C) or End (0x00) mean uncompressed
        case static_cast<int>(NbtTagType::Byte):
        case static_cast<int>(NbtTagType::Short):
        case static_cast<int>(NbtTagType::Int):
        case static_cast<int>(NbtTagType::Long):
        case static_cast<int>(NbtTagType::Float):
        case static_cast<int>(NbtTagType::Double):
        case static_cast<int>(NbtTagType::String):
        case static_cast<int>(NbtTagType::ByteArray):
        case static_cast<int>(NbtTagType::IntArray):
        case static_cast<int>(NbtTagType::LongArray):
        case static_cast<int>(NbtTagType::Compound):
        case static_cast<int>(NbtTagType::List):
        case static_cast<int>(NbtTagType::End):
            result = NbtCompression::None;
            break;

        case 0x1F:  // GZip magic number
            result = NbtCompression::GZip;
            break;

        case 0x78:  // ZLib header
            result = NbtCompression::ZLib;
            break;

        default:
            throw std::runtime_error("Could not auto-detect compression format (first byte: 0x" +
                                     std::to_string(first) + ")");
    }

    stream.seekg(pos);  // rewind
    return result;
}

// ─── Loading ─────────────────────────────────────────────────────────────────

int64_t NbtFile::load_from_file(const std::string& filepath) {
    return load_from_file(filepath, NbtCompression::AutoDetect, nullptr);
}

int64_t NbtFile::load_from_file(const std::string& filepath, NbtCompression compression,
                                TagSelector selector) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filepath);
    }

    int64_t result = load_from_stream(file, compression, selector);
    file_name_ = filepath;
    return result;
}

int64_t NbtFile::load_from_buffer(const uint8_t* buffer, size_t offset, size_t length,
                                  NbtCompression compression, TagSelector selector) {
    // Create an istream wrapping the buffer
    struct membuf : std::streambuf {
        membuf(const uint8_t* data, size_t len) {
            setg(const_cast<char*>(reinterpret_cast<const char*>(data)),
                 const_cast<char*>(reinterpret_cast<const char*>(data)),
                 const_cast<char*>(reinterpret_cast<const char*>(data)) + static_cast<std::streamsize>(len));
        }
    };
    membuf sb(buffer + offset, length);
    std::istream stream(&sb);
    return load_from_stream(stream, compression, selector);
}

int64_t NbtFile::load_from_stream(std::istream& stream, NbtCompression compression,
                                  TagSelector selector) {
    file_name_.clear();

    // Detect compression if needed
    if (compression == NbtCompression::AutoDetect) {
        file_compression_ = detect_compression(stream);
    } else {
        file_compression_ = compression;
    }

    // Track bytes read
    auto start_pos = stream.tellg();
    bool seekable = (start_pos >= 0);

    switch (file_compression_) {
        case NbtCompression::GZip: {
            // Use zlib's inflate with GZip auto-detection
            // We decompress the entire stream to a memory buffer first,
            // then parse from that buffer.
            std::vector<uint8_t> decompressed;
            constexpr size_t kChunkSize = 32768;

            z_stream zs = {};
            if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK) {  // 16+MAX_WBITS = gzip auto-detect
                throw std::runtime_error("GZip inflateInit failed");
            }

            std::vector<uint8_t> in(kChunkSize);
            int ret;
            do {
                stream.read(reinterpret_cast<char*>(in.data()), kChunkSize);
                auto got = static_cast<uInt>(stream.gcount());
                if (got == 0) break;

                zs.avail_in = got;
                zs.next_in = in.data();

                do {
                    std::vector<uint8_t> out(kChunkSize);
                    zs.avail_out = kChunkSize;
                    zs.next_out = out.data();

                    ret = inflate(&zs, Z_NO_FLUSH);
                    if (ret < 0 && ret != Z_BUF_ERROR) {
                        inflateEnd(&zs);
                        throw std::runtime_error("GZip decompression failed: " + std::to_string(ret));
                    }

                    auto have = kChunkSize - zs.avail_out;
                    decompressed.insert(decompressed.end(), out.data(), out.data() + have);
                } while (zs.avail_out == 0);
            } while (ret != Z_STREAM_END);

            inflateEnd(&zs);

            // Parse decompressed data
            struct membuf : std::streambuf {
                membuf(uint8_t* data, size_t len) {
                    setg(reinterpret_cast<char*>(data),
                         reinterpret_cast<char*>(data),
                         reinterpret_cast<char*>(data) + static_cast<std::streamsize>(len));
                }
            };
            membuf sb(decompressed.data(), decompressed.size());
            std::istream dec_stream(&sb);
            load_from_stream_internal(dec_stream, selector);
            break;
        }

        case NbtCompression::ZLib: {
            std::vector<uint8_t> decompressed;
            constexpr size_t kChunkSize = 32768;

            z_stream zs = {};
            // windowBits = 15 → ZLib format (expects RFC 1950 header + trailer)
            if (inflateInit(&zs) != Z_OK) {
                throw std::runtime_error("ZLib inflateInit failed");
            }

            std::vector<uint8_t> in(kChunkSize);
            int ret;
            do {
                stream.read(reinterpret_cast<char*>(in.data()), kChunkSize);
                auto got = static_cast<uInt>(stream.gcount());
                if (got == 0) break;

                zs.avail_in = got;
                zs.next_in = in.data();

                do {
                    std::vector<uint8_t> out(kChunkSize);
                    zs.avail_out = kChunkSize;
                    zs.next_out = out.data();

                    ret = inflate(&zs, Z_NO_FLUSH);
                    if (ret < 0 && ret != Z_BUF_ERROR) {
                        inflateEnd(&zs);
                        throw std::runtime_error("ZLib decompression failed: " + std::to_string(ret));
                    }

                    auto have = kChunkSize - zs.avail_out;
                    decompressed.insert(decompressed.end(), out.data(), out.data() + have);
                } while (zs.avail_out == 0);
            } while (ret != Z_STREAM_END);

            inflateEnd(&zs);

            struct membuf : std::streambuf {
                membuf(uint8_t* data, size_t len) {
                    setg(reinterpret_cast<char*>(data),
                         reinterpret_cast<char*>(data),
                         reinterpret_cast<char*>(data) + static_cast<std::streamsize>(len));
                }
            };
            membuf sb(decompressed.data(), decompressed.size());
            std::istream dec_stream(&sb);
            load_from_stream_internal(dec_stream, selector);
            break;
        }

        case NbtCompression::None:
            load_from_stream_internal(stream, selector);
            break;

        default:
            throw std::invalid_argument("Unsupported compression mode");
    }

    // Report bytes read
    if (seekable) {
        auto end_pos = stream.tellg();
        return (end_pos >= 0) ? (end_pos - start_pos) : 0;
    }
    return 0;
}

void NbtFile::load_from_stream_internal(std::istream& stream, TagSelector selector) {
    // First byte must be TAG_Compound
    int first = stream.get();
    if (first < 0) throw std::runtime_error("Unexpected end of stream");
    if (first != static_cast<int>(NbtTagType::Compound)) {
        throw NbtFormatException("NBT stream does not start with TAG_Compound");
    }

    NbtBinaryReader reader(stream, big_endian_);
    // TODO: wire up TagSelector through NbtBinaryReader if needed

    auto root = std::make_shared<NbtCompound>();
    root->set_name(reader.read_string());
    root->read_tag(reader);
    root_tag_ = root;
}

// ─── Saving ─────────────────────────────────────────────────────────────────

int64_t NbtFile::save_to_file(const std::string& filepath, NbtCompression compression) {
    validate_for_save(compression);
    std::ofstream file(filepath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot create file: " + filepath);
    }
    return save_to_stream(file, compression);
}

int64_t NbtFile::save_to_buffer(uint8_t* buffer, size_t capacity, NbtCompression compression) {
    validate_for_save(compression);
    struct outbuf : std::streambuf {
        uint8_t* begin;
        uint8_t* current;
        size_t remaining;

        outbuf(uint8_t* buf, size_t cap) : begin(buf), current(buf), remaining(cap) {}

        std::streamsize xsputn(const char_type* s, std::streamsize n) override {
            auto actual = std::min(static_cast<size_t>(n), remaining);
            std::memcpy(current, s, actual);
            current += actual;
            remaining -= actual;
            return actual;
        }

        int_type overflow(int_type c) override {
            if (remaining == 0) return traits_type::eof();
            *current++ = static_cast<char_type>(c);
            remaining--;
            return c;
        }
    };
    outbuf ob(buffer, capacity);
    std::ostream stream(&ob);
    return save_to_stream(stream, compression);
}

std::vector<uint8_t> NbtFile::save_to_buffer(NbtCompression compression) {
    validate_for_save(compression);
    std::ostringstream stream(std::ios::binary);
    save_to_stream(stream, compression);
    auto str = stream.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}

int64_t NbtFile::save_to_stream(std::ostream& stream, NbtCompression compression) {
    validate_for_save(compression);

    if (!root_tag_) {
        throw NbtFormatException("Cannot save: root tag is null");
    }

    auto start_pos = stream.tellp();

    switch (compression) {
        case NbtCompression::ZLib: {
            // Serialize NBT first
            std::ostringstream nbt_stream(std::ios::binary);
            NbtBinaryWriter writer(nbt_stream, big_endian_);
            root_tag_->write_tag(writer);
            auto nbt_data = nbt_stream.str();

            // Compress using zlib's deflate (ZLib format, RFC 1950)
            z_stream zs = {};
            // windowBits = 15 → ZLib header + deflate data + Adler32 trailer
            if (deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK) {
                throw std::runtime_error("ZLib deflateInit failed");
            }

            zs.avail_in = static_cast<uInt>(nbt_data.size());
            zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(nbt_data.data()));

            std::vector<uint8_t> out(kWriteBufferSize);
            int ret;
            do {
                zs.avail_out = kWriteBufferSize;
                zs.next_out = out.data();
                ret = deflate(&zs, Z_FINISH);
                auto have = kWriteBufferSize - zs.avail_out;
                stream.write(reinterpret_cast<const char*>(out.data()),
                             static_cast<std::streamsize>(have));
            } while (ret != Z_STREAM_END);

            deflateEnd(&zs);
            break;
        }

        case NbtCompression::GZip: {
            // Serialize NBT first
            std::ostringstream nbt_stream(std::ios::binary);
            NbtBinaryWriter writer(nbt_stream, big_endian_);
            root_tag_->write_tag(writer);
            auto nbt_data = nbt_stream.str();

            // Compress using zlib deflate with GZip wrapper
            z_stream zs = {};
            // 15 | 16 = gzip format
            if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                             15 | 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
                throw std::runtime_error("GZip deflateInit2 failed");
            }

            std::vector<uint8_t> out(kWriteBufferSize);
            zs.avail_in = static_cast<uInt>(nbt_data.size());
            zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(nbt_data.data()));

            int ret;
            do {
                zs.avail_out = kWriteBufferSize;
                zs.next_out = out.data();
                ret = deflate(&zs, Z_FINISH);
                auto have = kWriteBufferSize - zs.avail_out;
                stream.write(reinterpret_cast<const char*>(out.data()),
                             static_cast<std::streamsize>(have));
            } while (ret != Z_STREAM_END);

            deflateEnd(&zs);
            break;
        }

        case NbtCompression::None: {
            NbtBinaryWriter writer(stream, big_endian_);
            root_tag_->write_tag(writer);
            break;
        }

        default:
            throw std::invalid_argument("Unsupported compression mode");
    }

    auto end_pos = stream.tellp();
    return (end_pos >= 0) ? (end_pos - start_pos) : 0;
}

// ─── Utilities ───────────────────────────────────────────────────────────────

std::string NbtFile::read_root_tag_name(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filepath);
    }

    auto compression = detect_compression(file);

    switch (compression) {
        case NbtCompression::GZip: {
            z_stream zs = {};
            if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK) {
                throw std::runtime_error("inflateInit failed");
            }
            std::vector<uint8_t> in(8192);
            std::vector<uint8_t> out(8192);
            std::vector<uint8_t> decompressed;

            int ret;
            do {
                file.read(reinterpret_cast<char*>(in.data()), in.size());
                auto got = static_cast<uInt>(file.gcount());
                if (got == 0) break;

                zs.avail_in = got;
                zs.next_in = in.data();

                do {
                    zs.avail_out = static_cast<uInt>(out.size());
                    zs.next_out = out.data();
                    ret = inflate(&zs, Z_NO_FLUSH);
                    if (ret < 0) { inflateEnd(&zs); throw std::runtime_error("inflate failed"); }
                    auto have = out.size() - zs.avail_out;
                    decompressed.insert(decompressed.end(), out.data(), out.data() + have);
                } while (zs.avail_out == 0);
            } while (ret != Z_STREAM_END);
            inflateEnd(&zs);

            struct membuf : std::streambuf {
                membuf(uint8_t* d, size_t len) {
                    setg(reinterpret_cast<char*>(d), reinterpret_cast<char*>(d),
                         reinterpret_cast<char*>(d) + static_cast<std::streamsize>(len));
                }
            };
            membuf sb(decompressed.data(), decompressed.size());
            std::istream dec(&sb);
            return get_root_name_internal(dec, s_big_endian_by_default);
        }

        case NbtCompression::ZLib: {
            z_stream zs = {};
            if (inflateInit(&zs) != Z_OK) throw std::runtime_error("inflateInit failed");
            std::vector<uint8_t> in(8192);
            std::vector<uint8_t> out(8192);
            std::vector<uint8_t> decompressed;
            int ret;
            do {
                file.read(reinterpret_cast<char*>(in.data()), in.size());
                auto got = static_cast<uInt>(file.gcount());
                if (got == 0) break;
                zs.avail_in = got;
                zs.next_in = in.data();
                do {
                    zs.avail_out = static_cast<uInt>(out.size());
                    zs.next_out = out.data();
                    ret = inflate(&zs, Z_NO_FLUSH);
                    if (ret < 0) { inflateEnd(&zs); throw std::runtime_error("inflate failed"); }
                    auto have = out.size() - zs.avail_out;
                    decompressed.insert(decompressed.end(), out.data(), out.data() + have);
                } while (zs.avail_out == 0);
            } while (ret != Z_STREAM_END);
            inflateEnd(&zs);
            struct membuf : std::streambuf {
                membuf(uint8_t* d, size_t len) {
                    setg(reinterpret_cast<char*>(d), reinterpret_cast<char*>(d),
                         reinterpret_cast<char*>(d) + static_cast<std::streamsize>(len));
                }
            };
            membuf sb(decompressed.data(), decompressed.size());
            std::istream dec(&sb);
            return get_root_name_internal(dec, s_big_endian_by_default);
        }

        case NbtCompression::None:
            return get_root_name_internal(file, s_big_endian_by_default);

        default:
            throw std::invalid_argument("Unknown compression");
    }
}

std::string NbtFile::get_root_name_internal(std::istream& stream, bool big_endian) {
    int first = stream.get();
    if (first < 0) throw std::runtime_error("Unexpected end of stream");
    if (first != static_cast<int>(NbtTagType::Compound)) {
        throw NbtFormatException("NBT stream does not start with TAG_Compound");
    }
    NbtBinaryReader reader(stream, big_endian);
    return reader.read_string();
}

std::string NbtFile::to_string() const {
    return root_tag_->to_string();
}

std::string NbtFile::to_string(const std::string& indent) const {
    return root_tag_->to_string(indent);
}

} // namespace nbtcpp
