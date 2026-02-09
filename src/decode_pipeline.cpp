#include "decode_pipeline.hpp"

#include <cmath>

#include <btccw/base43.hpp>
#include <btccw/transaction.hpp>

#include "audio_io.hpp"

namespace btccw::node {

DecodePipeline::DecodePipeline(double sample_rate, double tone_freq, int wpm,
                               std::size_t block_size, double threshold)
    : detector_(sample_rate, tone_freq, block_size, threshold),
      morse_decoder_(static_cast<int>(
          std::round(AudioIO::unit_duration(wpm) * sample_rate /
                     static_cast<double>(block_size)))) {}

DecodeResult DecodePipeline::decode(const std::vector<float>& pcm) const {
    DecodeResult result;

    // Stage 1: Goertzel tone detection.
    result.stage_reached = DecodeStage::Goertzel;
    result.tone_bits = detector_.detect(pcm);
    if (result.tone_bits.empty()) {
        result.error = "Goertzel: no blocks to analyze";
        return result;
    }

    // Stage 2: Morse decode.
    result.stage_reached = DecodeStage::MorseDecode;
    result.morse_text = morse_decoder_.decode(result.tone_bits);
    if (result.morse_text.empty()) {
        result.error = "Morse decode: no text recovered";
        return result;
    }

    // Stage 3: Deframe (strip KKK/AR, verify CRC).
    result.stage_reached = DecodeStage::Deframe;
    auto deframe_result = Deframer::deframe(result.morse_text);
    if (!deframe_result.valid) {
        result.error = "Deframe: " + deframe_result.error;
        return result;
    }
    result.base43_payload = deframe_result.payload;

    // Stage 4: Base43 decode.
    result.stage_reached = DecodeStage::Base43Decode;
    result.raw_bytes = btccw::Base43::decode(result.base43_payload);
    if (result.raw_bytes.empty()) {
        result.error = "Base43 decode: invalid encoding";
        return result;
    }

    // Stage 5: Convert to hex and validate.
    result.stage_reached = DecodeStage::Validate;
    result.hex_string = btccw::Transaction::bytes_to_hex(
        result.raw_bytes.data(), result.raw_bytes.size());

    if (!btccw::Transaction::validate(result.hex_string)) {
        result.error = "Transaction validation failed";
        return result;
    }

    result.stage_reached = DecodeStage::Complete;
    result.success = true;
    return result;
}

} // namespace btccw::node
