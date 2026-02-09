#include "goertzel.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace btccw::node {

GoertzelDetector::GoertzelDetector(double sample_rate, double tone_freq,
                                   std::size_t block_size, double threshold)
    : sample_rate_(sample_rate),
      tone_freq_(tone_freq),
      block_size_(block_size),
      threshold_(threshold) {
    // k = round(N * f / fs) — integer bin index for bin-centered detection
    double k = std::round(block_size_ * tone_freq_ / sample_rate_);
    coeff_ = 2.0 * std::cos(2.0 * M_PI * k / static_cast<double>(block_size_));
}

double GoertzelDetector::magnitude(const float* samples, std::size_t count) const {
    double s0 = 0.0;
    double s1 = 0.0;
    double s2 = 0.0;

    for (std::size_t i = 0; i < count; ++i) {
        s0 = static_cast<double>(samples[i]) + coeff_ * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    // Power = s1^2 + s2^2 - coeff * s1 * s2
    return s1 * s1 + s2 * s2 - coeff_ * s1 * s2;
}

std::vector<bool> GoertzelDetector::detect(const std::vector<float>& pcm) const {
    if (pcm.empty() || block_size_ == 0) return {};

    std::size_t num_blocks = pcm.size() / block_size_;
    if (num_blocks == 0) return {};

    // Compute magnitudes for all blocks.
    std::vector<double> mags(num_blocks);
    for (std::size_t i = 0; i < num_blocks; ++i) {
        mags[i] = magnitude(pcm.data() + i * block_size_, block_size_);
    }

    // Determine threshold: use provided value or auto-compute from median.
    double thresh_on = threshold_;
    if (thresh_on <= 0.0) {
        std::vector<double> sorted = mags;
        std::sort(sorted.begin(), sorted.end());
        double median = sorted[sorted.size() / 2];
        thresh_on = median * 3.0;
    }

    // Hysteresis: OFF threshold is 70% of ON threshold.
    double thresh_off = thresh_on * 0.7;

    // Apply hysteresis thresholding.
    std::vector<bool> result(num_blocks);
    bool state = false; // start OFF

    for (std::size_t i = 0; i < num_blocks; ++i) {
        if (state) {
            // Currently ON — stay ON unless below OFF threshold.
            if (mags[i] < thresh_off) {
                state = false;
            }
        } else {
            // Currently OFF — turn ON if above ON threshold.
            if (mags[i] >= thresh_on) {
                state = true;
            }
        }
        result[i] = state;
    }

    return result;
}

} // namespace btccw::node
