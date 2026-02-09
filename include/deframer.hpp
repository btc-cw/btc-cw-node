#ifndef BTCCW_NODE_DEFRAMER_HPP
#define BTCCW_NODE_DEFRAMER_HPP

#include <string>

namespace btccw::node {

/// Result of a deframe operation.
struct DeframeResult {
    bool        valid   = false;
    std::string payload;
    std::string error;
};

/// Inverse of Checksum::frame().
///
/// Frame format: "KKK " + payload + encode_crc(crc32(payload)) + " AR"
/// No space between payload and CRC. The last 4 chars before " AR" are the CRC.
class Deframer {
public:
    /// Strip framing, extract payload, and verify CRC.
    static DeframeResult deframe(const std::string& text);
};

} // namespace btccw::node

#endif // BTCCW_NODE_DEFRAMER_HPP
