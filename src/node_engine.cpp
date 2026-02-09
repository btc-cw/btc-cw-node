#include "node_engine.hpp"

#include <cstdio>

#include <btccw/base43.hpp>
#include <btccw/checksum.hpp>
#include <btccw/morse.hpp>
#include <btccw/transaction.hpp>

namespace btccw::node {

bool NodeEngine::init(const AudioConfig& audio_cfg,
                      const GatewayConfig& gw_cfg) {
    if (!audio_.open(audio_cfg)) {
        std::fprintf(stderr, "[engine] audio init failed\n");
        return false;
    }
    if (!gateway_.open(gw_cfg)) {
        std::fprintf(stderr, "[engine] gateway init failed\n");
        return false;
    }

    // Construct the decode pipeline with audio config params.
    decode_pipeline_ = std::make_unique<DecodePipeline>(
        audio_cfg.sample_rate, audio_cfg.tone_freq_hz,
        audio_cfg.wpm);

    return true;
}

void NodeEngine::shutdown() {
    decode_pipeline_.reset();
    audio_.close();
    gateway_.close();
}

// ---------------------------------------------------------------------------
// Transmit path
// ---------------------------------------------------------------------------

std::vector<int8_t> NodeEngine::encode_tx(std::string_view raw_tx_hex) {
    // 1. Validate the transaction structure & signatures.
    if (!btccw::Transaction::validate(raw_tx_hex)) {
        std::fprintf(stderr, "[engine] transaction validation failed\n");
        return {};
    }

    // 2. Convert hex to raw bytes, then Base43-encode.
    auto raw_bytes = btccw::Transaction::hex_to_bytes(raw_tx_hex);
    std::string b43 = btccw::Base43::encode(raw_bytes);

    // 3. Wrap in protocol frame: KKK <payload><crc> AR
    std::string framed = btccw::Checksum::frame(b43);

    std::printf("[engine] framed payload: %zu chars\n", framed.size());

    // 4. Convert to Morse timing array.
    return btccw::MorseEncoder::encode(framed);
}

bool NodeEngine::play(const std::vector<int8_t>& timing) {
    return audio_.transmit(timing);
}

bool NodeEngine::transmit(std::string_view raw_tx_hex) {
    auto timing = encode_tx(raw_tx_hex);
    if (timing.empty()) return false;
    return play(timing);
}

// ---------------------------------------------------------------------------
// Receive path
// ---------------------------------------------------------------------------

std::vector<float> NodeEngine::listen(double duration_sec) {
    return audio_.capture(duration_sec);
}

DecodeResult NodeEngine::decode_audio(const std::vector<float>& pcm) {
    if (!decode_pipeline_) {
        return {DecodeStage::None, false, {}, {}, {}, {}, {},
                "decode pipeline not initialized"};
    }
    return decode_pipeline_->decode(pcm);
}

DecodeResult NodeEngine::listen_and_decode(double duration_sec) {
    auto pcm = listen(duration_sec);
    return decode_audio(pcm);
}

// ---------------------------------------------------------------------------
// Network
// ---------------------------------------------------------------------------

std::string NodeEngine::broadcast(std::string_view raw_tx_hex) {
    if (!btccw::Transaction::validate(raw_tx_hex)) {
        std::fprintf(stderr, "[engine] refusing to broadcast invalid TX\n");
        return {};
    }
    return gateway_.broadcast(raw_tx_hex);
}

} // namespace btccw::node
