#ifndef BTCCW_NODE_MORSE_DECODER_HPP
#define BTCCW_NODE_MORSE_DECODER_HPP

#include <string>
#include <unordered_map>
#include <vector>

namespace btccw::node {

/// Decodes a boolean tone stream (from GoertzelDetector) back to text.
///
/// Uses MorseEncoder::lookup() to build a reverse table at init — no
/// duplicated Morse tables.
class MorseDecoder {
public:
    /// @param blocks_per_unit  Number of Goertzel blocks per Morse timing unit.
    ///                         Typically ~3 (unit_duration / block_duration).
    explicit MorseDecoder(int blocks_per_unit = 3);

    /// Decode a boolean tone stream to text.
    std::string decode(const std::vector<bool>& tones) const;

private:
    int blocks_per_unit_;

    /// Reverse lookup: Morse pattern (e.g. ".-") -> character.
    std::unordered_map<std::string, char> reverse_table_;

    /// Build reverse_table_ from MorseEncoder::lookup().
    void build_reverse_table();
};

} // namespace btccw::node

#endif // BTCCW_NODE_MORSE_DECODER_HPP
