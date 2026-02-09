#include <cstdio>
#include <cstring>
#include <string>

#include <btccw/btccw.hpp>

#include "node_engine.hpp"

static void print_usage() {
    std::puts(
        "btc-cw-node v1.0.0\n"
        "Usage:\n"
        "  btc-cw-node tx <raw_hex>      Validate, encode, and transmit a TX via audio\n"
        "  btc-cw-node listen <seconds>   Capture audio from the mic\n"
        "  btc-cw-node broadcast <hex>    Broadcast a raw TX to the Bitcoin network\n"
        "  btc-cw-node devices            List available audio devices\n"
        "  btc-cw-node loopback <hex>     Full acoustic loopback test\n"
    );
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

static int cmd_tx(btccw::node::NodeEngine& engine, const char* hex) {
    auto timing = engine.encode_tx(hex);
    if (timing.empty()) {
        std::fprintf(stderr, "error: invalid or unsigned transaction\n");
        return 1;
    }

    std::printf("[tx] encoded %zu morse timing units\n", timing.size());

    if (!engine.play(timing)) {
        std::fprintf(stderr, "error: audio playback failed\n");
        return 1;
    }

    std::puts("[tx] transmission complete");
    return 0;
}

static const char* stage_name(btccw::node::DecodeStage stage) {
    switch (stage) {
        case btccw::node::DecodeStage::None:         return "none";
        case btccw::node::DecodeStage::Goertzel:     return "goertzel";
        case btccw::node::DecodeStage::MorseDecode:  return "morse_decode";
        case btccw::node::DecodeStage::Deframe:      return "deframe";
        case btccw::node::DecodeStage::Base43Decode:  return "base43_decode";
        case btccw::node::DecodeStage::Validate:     return "validate";
        case btccw::node::DecodeStage::Complete:      return "complete";
    }
    return "unknown";
}

static int cmd_listen(btccw::node::NodeEngine& engine, double seconds) {
    std::printf("[listen] capturing %.1f seconds of audio...\n", seconds);
    auto pcm = engine.listen(seconds);
    std::printf("[listen] captured %zu samples\n", pcm.size());

    auto result = engine.decode_audio(pcm);
    if (result.success) {
        std::printf("[listen] decoded TX: %s\n", result.hex_string.c_str());
    } else {
        std::fprintf(stderr, "[listen] decode failed at stage '%s': %s\n",
                     stage_name(result.stage_reached), result.error.c_str());
        if (!result.morse_text.empty()) {
            std::fprintf(stderr, "[listen] morse text: %s\n",
                         result.morse_text.c_str());
        }
    }
    return result.success ? 0 : 1;
}

static int cmd_broadcast(btccw::node::NodeEngine& engine, const char* hex) {
    std::printf("[broadcast] sending to network...\n");
    std::string txid = engine.broadcast(hex);
    if (txid.empty()) {
        std::fprintf(stderr, "error: broadcast failed\n");
        return 1;
    }
    std::printf("[broadcast] success — txid: %s\n", txid.c_str());
    return 0;
}

static int cmd_loopback(btccw::node::NodeEngine& engine, const char* hex) {
    std::puts("=== Acoustic Loopback Test ===\n");

    // 1. Validate & encode
    auto timing = engine.encode_tx(hex);
    if (timing.empty()) {
        std::fprintf(stderr, "error: invalid transaction\n");
        return 1;
    }
    std::printf("[1/4] encoded %zu timing units\n", timing.size());

    // 2. Transmit
    if (!engine.play(timing)) {
        std::fprintf(stderr, "error: playback failed\n");
        return 1;
    }
    std::puts("[2/4] audio transmitted");

    // 3. Capture (placeholder duration based on timing length)
    double duration = static_cast<double>(timing.size()) *
                      btccw::node::AudioIO::unit_duration(20) + 0.5;
    auto pcm = engine.listen(duration);
    std::printf("[3/4] captured %zu samples\n", pcm.size());

    // 4. Decode
    auto result = engine.decode_audio(pcm);
    if (result.success) {
        std::printf("[4/4] decoded TX: %s\n", result.hex_string.c_str());
        if (result.hex_string == hex) {
            std::puts("\n=== PASS — roundtrip matches ===");
        } else {
            std::puts("\n=== MISMATCH — decoded hex differs from input ===");
            return 1;
        }
    } else {
        std::fprintf(stderr, "[4/4] decode failed at stage '%s': %s\n",
                     stage_name(result.stage_reached), result.error.c_str());
        if (!result.morse_text.empty()) {
            std::fprintf(stderr, "      morse text: %s\n",
                         result.morse_text.c_str());
        }
        return 1;
    }

    std::puts("\n=== Loopback test finished ===");
    return 0;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char* cmd = argv[1];

    if (std::strcmp(cmd, "devices") == 0) {
        btccw::node::AudioIO::list_devices();
        return 0;
    }

    // Initialise the engine with default config.
    btccw::node::NodeEngine engine;
    btccw::node::AudioConfig audio_cfg;
    btccw::node::GatewayConfig gw_cfg;

    if (!engine.init(audio_cfg, gw_cfg)) {
        std::fprintf(stderr, "error: failed to initialise engine\n");
        return 1;
    }

    int rc = 1;

    if (std::strcmp(cmd, "tx") == 0 && argc >= 3) {
        rc = cmd_tx(engine, argv[2]);
    } else if (std::strcmp(cmd, "listen") == 0 && argc >= 3) {
        rc = cmd_listen(engine, std::stod(argv[2]));
    } else if (std::strcmp(cmd, "broadcast") == 0 && argc >= 3) {
        rc = cmd_broadcast(engine, argv[2]);
    } else if (std::strcmp(cmd, "loopback") == 0 && argc >= 3) {
        rc = cmd_loopback(engine, argv[2]);
    } else {
        print_usage();
    }

    engine.shutdown();
    return rc;
}
