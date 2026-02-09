#include "audio_io.hpp"

#include <cmath>
#include <cstdio>

namespace btccw::node {

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

AudioIO::AudioIO() = default;

AudioIO::~AudioIO() { close(); }

bool AudioIO::open(const AudioConfig& cfg) {
    cfg_ = cfg;

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::fprintf(stderr, "[audio] Pa_Initialize failed: %s\n",
                     Pa_GetErrorText(err));
        return false;
    }
    initialized_ = true;

    // --- output stream ---
    PaStreamParameters out_params{};
    out_params.device = (cfg_.output_device >= 0)
                            ? cfg_.output_device
                            : Pa_GetDefaultOutputDevice();
    out_params.channelCount = 1;
    out_params.sampleFormat = paFloat32;
    out_params.suggestedLatency =
        Pa_GetDeviceInfo(out_params.device)->defaultLowOutputLatency;

    err = Pa_OpenStream(&output_stream_, nullptr, &out_params,
                        cfg_.sample_rate, paFramesPerBufferUnspecified,
                        paClipOff, nullptr, nullptr);
    if (err != paNoError) {
        std::fprintf(stderr, "[audio] output stream open failed: %s\n",
                     Pa_GetErrorText(err));
        return false;
    }

    // --- input stream ---
    PaStreamParameters in_params{};
    in_params.device = (cfg_.input_device >= 0)
                           ? cfg_.input_device
                           : Pa_GetDefaultInputDevice();
    in_params.channelCount = 1;
    in_params.sampleFormat = paFloat32;
    in_params.suggestedLatency =
        Pa_GetDeviceInfo(in_params.device)->defaultLowInputLatency;

    err = Pa_OpenStream(&input_stream_, &in_params, nullptr,
                        cfg_.sample_rate, paFramesPerBufferUnspecified,
                        paClipOff, nullptr, nullptr);
    if (err != paNoError) {
        std::fprintf(stderr, "[audio] input stream open failed: %s\n",
                     Pa_GetErrorText(err));
        // Input is non-fatal — transmit-only mode is still useful.
        input_stream_ = nullptr;
    }

    return true;
}

void AudioIO::close() {
    if (output_stream_) { Pa_CloseStream(output_stream_); output_stream_ = nullptr; }
    if (input_stream_)  { Pa_CloseStream(input_stream_);  input_stream_  = nullptr; }
    if (initialized_)   { Pa_Terminate(); initialized_ = false; }
}

// ---------------------------------------------------------------------------
// Transmit
// ---------------------------------------------------------------------------

bool AudioIO::transmit(const std::vector<int8_t>& timing) {
    if (!output_stream_) return false;

    std::vector<float> pcm = render_tone(timing);

    PaError err = Pa_StartStream(output_stream_);
    if (err != paNoError) return false;

    err = Pa_WriteStream(output_stream_, pcm.data(),
                         static_cast<unsigned long>(pcm.size()));

    Pa_StopStream(output_stream_);
    return err == paNoError;
}

// ---------------------------------------------------------------------------
// Capture
// ---------------------------------------------------------------------------

std::vector<float> AudioIO::capture(double duration_sec) {
    if (!input_stream_) return {};

    auto num_frames = static_cast<unsigned long>(cfg_.sample_rate * duration_sec);
    std::vector<float> buf(num_frames, 0.0f);

    PaError err = Pa_StartStream(input_stream_);
    if (err != paNoError) return {};

    Pa_ReadStream(input_stream_, buf.data(), num_frames);
    Pa_StopStream(input_stream_);

    return buf;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

double AudioIO::unit_duration(int wpm) {
    // PARIS standard: 50 units per word.
    return 1.2 / static_cast<double>(wpm);
}

void AudioIO::list_devices() {
    Pa_Initialize();
    int n = Pa_GetDeviceCount();
    for (int i = 0; i < n; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        std::printf("  [%d] %s  (in:%d out:%d)\n",
                    i, info->name,
                    info->maxInputChannels,
                    info->maxOutputChannels);
    }
    Pa_Terminate();
}

std::vector<float> AudioIO::render_tone(const std::vector<int8_t>& timing) const {
    const double unit_sec = unit_duration(cfg_.wpm);
    const auto   samples_per_unit =
        static_cast<std::size_t>(cfg_.sample_rate * unit_sec);

    std::vector<float> pcm;
    pcm.reserve(timing.size() * samples_per_unit);

    const double omega = 2.0 * M_PI * cfg_.tone_freq_hz / cfg_.sample_rate;
    std::size_t  sample_idx = 0;

    for (int8_t t : timing) {
        for (std::size_t s = 0; s < samples_per_unit; ++s, ++sample_idx) {
            if (t > 0) {
                pcm.push_back(static_cast<float>(
                    0.8 * std::sin(omega * static_cast<double>(sample_idx))));
            } else {
                pcm.push_back(0.0f);
            }
        }
    }
    return pcm;
}

} // namespace btccw::node
