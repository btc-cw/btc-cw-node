#include "deframer.hpp"

#include <btccw/checksum.hpp>

namespace btccw::node {

DeframeResult Deframer::deframe(const std::string& text) {
    // Frame format: "KKK " + payload + crc(4 chars) + " AR"
    // Minimum length: 4 (prefix) + 0 (payload) + 4 (crc) + 3 (suffix) = 11
    static constexpr std::size_t kPrefixLen = 4;  // "KKK "
    static constexpr std::size_t kSuffixLen = 3;  // " AR"
    static constexpr std::size_t kCrcLen    = 4;
    static constexpr std::size_t kMinLen    = kPrefixLen + kCrcLen + kSuffixLen;

    if (text.size() < kMinLen) {
        return {false, {}, "frame too short"};
    }

    // Check prefix "KKK "
    if (text.substr(0, kPrefixLen) != "KKK ") {
        return {false, {}, "missing KKK preamble"};
    }

    // Check suffix " AR"
    if (text.substr(text.size() - kSuffixLen) != " AR") {
        return {false, {}, "missing AR prosign"};
    }

    // Extract body (between prefix and suffix).
    std::string body = text.substr(kPrefixLen, text.size() - kPrefixLen - kSuffixLen);

    if (body.size() < kCrcLen) {
        return {false, {}, "body too short for CRC"};
    }

    // Split body into payload and CRC.
    std::string payload      = body.substr(0, body.size() - kCrcLen);
    std::string received_crc = body.substr(body.size() - kCrcLen);

    // Verify CRC.
    uint32_t computed = btccw::Checksum::crc32(payload);
    std::string expected_crc = btccw::Checksum::encode_crc(computed);

    if (received_crc != expected_crc) {
        return {false, payload, "CRC mismatch: expected " + expected_crc +
                                ", got " + received_crc};
    }

    return {true, payload, {}};
}

} // namespace btccw::node
