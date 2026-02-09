#ifndef BTCCW_NODE_ENGINE_HPP
#define BTCCW_NODE_ENGINE_HPP

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "audio_io.hpp"
#include "decode_pipeline.hpp"
#include "gateway.hpp"

namespace btccw::node {

/// Top-level orchestrator that wires Core, Audio, and Network together.
///
/// Transmit path:
///   raw_tx_hex -> validate -> base43 encode -> frame (CRC) -> morse timing
///              -> audio out (PortAudio)
///
/// Receive path:
///   audio in (mic / SDR) -> FFTW Goertzel detect -> morse decode
///              -> deframe -> base43 decode -> validate -> broadcast
class NodeEngine {
public:
    NodeEngine() = default;

    /// Initialise all subsystems.
    bool init(const AudioConfig& audio_cfg,
              const GatewayConfig& gw_cfg);

    /// Shut down all subsystems.
    void shutdown();

    // ----- Transmit path -----

    /// Encode a raw transaction hex into a framed Morse timing array.
    /// Returns the timing array, or empty on validation failure.
    std::vector<int8_t> encode_tx(std::string_view raw_tx_hex);

    /// Play the encoded timing array as audio.
    bool play(const std::vector<int8_t>& timing);

    /// One-shot: validate, encode, frame, and play a raw transaction.
    bool transmit(std::string_view raw_tx_hex);

    // ----- Receive path -----

    /// Capture audio from the mic for `duration_sec` and return raw PCM.
    std::vector<float> listen(double duration_sec);

    /// Decode a PCM buffer through the full receive pipeline.
    DecodeResult decode_audio(const std::vector<float>& pcm);

    /// Capture audio and decode in one step.
    DecodeResult listen_and_decode(double duration_sec);

    // ----- Network -----

    /// Broadcast a validated raw transaction to the Bitcoin network.
    std::string broadcast(std::string_view raw_tx_hex);

private:
    AudioIO audio_;
    Gateway gateway_;
    std::unique_ptr<DecodePipeline> decode_pipeline_;
};

} // namespace btccw::node

#endif // BTCCW_NODE_ENGINE_HPP
