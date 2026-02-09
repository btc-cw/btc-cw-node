#ifndef BTCCW_NODE_AUDIO_IO_HPP
#define BTCCW_NODE_AUDIO_IO_HPP

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <portaudio.h>

namespace btccw::node {

/// Configuration for audio I/O.
struct AudioConfig {
    double sample_rate   = 44100.0;
    double tone_freq_hz  = 750.0;    // CW tone frequency
    int    wpm           = 20;       // Words per minute
    int    output_device = -1;       // -1 = default
    int    input_device  = -1;       // -1 = default
};

/// PortAudio wrapper for transmitting and receiving Morse audio.
class AudioIO {
public:
    AudioIO();
    ~AudioIO();

    AudioIO(const AudioIO&) = delete;
    AudioIO& operator=(const AudioIO&) = delete;

    /// Initialise PortAudio and select devices.
    bool open(const AudioConfig& cfg);

    /// Shut down PortAudio.
    void close();

    /// Play a Morse timing array as audio through the output device.
    /// Each element is +1 (tone ON) or -1 (silence) for one timing unit.
    bool transmit(const std::vector<int8_t>& timing);

    /// Record audio from the input device for `duration_sec` seconds.
    /// Returns the captured PCM samples (mono, float).
    std::vector<float> capture(double duration_sec);

    /// List available audio devices and their indices.
    static void list_devices();

    /// Compute the duration of one timing unit in seconds for a given WPM.
    static double unit_duration(int wpm);

private:
    PaStream*   output_stream_ = nullptr;
    PaStream*   input_stream_  = nullptr;
    AudioConfig cfg_;
    bool        initialized_   = false;

    /// Render the timing array into a PCM buffer of sine-wave samples.
    std::vector<float> render_tone(const std::vector<int8_t>& timing) const;
};

} // namespace btccw::node

#endif // BTCCW_NODE_AUDIO_IO_HPP
