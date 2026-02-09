#ifndef BTCCW_NODE_DECODE_PIPELINE_HPP
#define BTCCW_NODE_DECODE_PIPELINE_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "deframer.hpp"
#include "goertzel.hpp"
#include "morse_decoder.hpp"

namespace btccw::node {

/// Stages of the decode pipeline.
enum class DecodeStage {
    None,
    Goertzel,
    MorseDecode,
    Deframe,
    Base43Decode,
    Validate,
    Complete
};

/// Result from the full decode pipeline, with staged error reporting.
struct DecodeResult {
    DecodeStage stage_reached = DecodeStage::None;
    bool        success       = false;

    // Intermediate values (populated as stages complete).
    std::vector<bool> tone_bits;
    std::string       morse_text;
    std::string       base43_payload;
    std::vector<uint8_t> raw_bytes;
    std::string       hex_string;

    std::string error;
};

/// Full receive/decode pipeline: PCM → hex transaction.
///
/// Stages:
///   1. Goertzel detect → vector<bool>
///   2. Morse decode → text string
///   3. Deframe → Base43 payload (CRC verified)
///   4. Base43::decode() → raw bytes
///   5. Transaction::bytes_to_hex() + validate() → hex string
class DecodePipeline {
public:
    /// Construct the pipeline with audio parameters.
    /// @param sample_rate   Audio sample rate (e.g. 44100)
    /// @param tone_freq     CW tone frequency (e.g. 750)
    /// @param wpm           Words per minute (e.g. 20)
    /// @param block_size    Goertzel block size (e.g. 882)
    /// @param threshold     Goertzel threshold (0 = auto)
    DecodePipeline(double sample_rate, double tone_freq, int wpm,
                   std::size_t block_size = 882, double threshold = 0.0);

    /// Run the full pipeline on a PCM buffer.
    DecodeResult decode(const std::vector<float>& pcm) const;

private:
    GoertzelDetector detector_;
    MorseDecoder     morse_decoder_;
};

} // namespace btccw::node

#endif // BTCCW_NODE_DECODE_PIPELINE_HPP
