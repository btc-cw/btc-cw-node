#ifdef BTCCW_HAS_SDR

#include "sdr_input.hpp"

#include <cstdio>

namespace btccw::node {

SdrInput::SdrInput() = default;

SdrInput::~SdrInput() { close(); }

bool SdrInput::open(const SdrConfig& cfg) {
    if (rtlsdr_open(&dev_, static_cast<uint32_t>(cfg.device_index)) < 0) {
        std::fprintf(stderr, "[sdr] failed to open device %d\n",
                     cfg.device_index);
        return false;
    }

    rtlsdr_set_center_freq(dev_, cfg.center_freq_hz);
    rtlsdr_set_sample_rate(dev_, cfg.sample_rate);

    if (cfg.gain_db == 0) {
        rtlsdr_set_tuner_gain_mode(dev_, 0); // auto gain
    } else {
        rtlsdr_set_tuner_gain_mode(dev_, 1);
        rtlsdr_set_tuner_gain(dev_, cfg.gain_db * 10); // tenths of dB
    }

    rtlsdr_reset_buffer(dev_);
    open_ = true;
    return true;
}

void SdrInput::close() {
    if (open_ && dev_) {
        rtlsdr_close(dev_);
        dev_  = nullptr;
        open_ = false;
    }
}

int SdrInput::read_sync(std::vector<uint8_t>& buffer, int num_bytes) {
    if (!open_) return -1;
    buffer.resize(static_cast<std::size_t>(num_bytes));
    int n_read = 0;
    int rc = rtlsdr_read_sync(dev_, buffer.data(), num_bytes, &n_read);
    if (rc < 0) return -1;
    buffer.resize(static_cast<std::size_t>(n_read));
    return n_read;
}

bool SdrInput::device_available() { return device_count() > 0; }

int SdrInput::device_count() {
    return static_cast<int>(rtlsdr_get_device_count());
}

} // namespace btccw::node

#endif // BTCCW_HAS_SDR
