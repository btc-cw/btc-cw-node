#ifndef BTCCW_NODE_GOERTZEL_HPP
#define BTCCW_NODE_GOERTZEL_HPP

#include <cstddef>
#include <vector>

namespace btccw::node {

/// Single-frequency tone detector using the Goertzel algorithm.
///
/// Processes mono PCM in fixed-size blocks and outputs a boolean stream
/// indicating tone present/absent per block.
class GoertzelDetector {
public:
    /// Construct a detector for the given frequency.
    /// @param sample_rate  Audio sample rate (e.g. 44100)
    /// @param tone_freq    Target frequency in Hz (e.g. 750)
    /// @param block_size   Samples per analysis block (e.g. 882 for ~20ms at 44100 Hz)
    /// @param threshold    Detection threshold; 0 = auto (median * 3.0)
    GoertzelDetector(double sample_rate, double tone_freq,
                     std::size_t block_size = 882, double threshold = 0.0);

    /// Process a PCM buffer and return tone present/absent per block.
    std::vector<bool> detect(const std::vector<float>& pcm) const;

    std::size_t block_size() const noexcept { return block_size_; }

private:
    double      sample_rate_;
    double      tone_freq_;
    std::size_t block_size_;
    double      threshold_;
    double      coeff_;       // 2 * cos(2π * k / N)

    /// Compute the Goertzel magnitude for a single block.
    double magnitude(const float* samples, std::size_t count) const;
};

} // namespace btccw::node

#endif // BTCCW_NODE_GOERTZEL_HPP
