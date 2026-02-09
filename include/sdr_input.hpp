#ifndef BTCCW_NODE_SDR_INPUT_HPP
#define BTCCW_NODE_SDR_INPUT_HPP

#ifdef BTCCW_HAS_SDR

#include <cstdint>
#include <functional>
#include <vector>

#include <rtl-sdr.h>

namespace btccw::node {

/// Configuration for RTL-SDR receiver.
struct SdrConfig {
    uint32_t center_freq_hz = 7030000;   // 40m CW band default
    uint32_t sample_rate    = 2400000;   // 2.4 MHz
    int      gain_db        = 40;        // RF gain (0 = auto)
    int      device_index   = 0;
};

/// RTL-SDR wrapper for receiving CW signals off-air.
class SdrInput {
public:
    SdrInput();
    ~SdrInput();

    SdrInput(const SdrInput&) = delete;
    SdrInput& operator=(const SdrInput&) = delete;

    /// Open the RTL-SDR device and configure tuner.
    bool open(const SdrConfig& cfg);

    /// Close the device.
    void close();

    /// Read a block of raw I/Q samples (interleaved uint8_t).
    /// Returns the number of bytes actually read.
    int read_sync(std::vector<uint8_t>& buffer, int num_bytes);

    /// Check whether a device is connected.
    static bool device_available();

    /// Return the number of RTL-SDR devices found.
    static int device_count();

private:
    rtlsdr_dev_t* dev_ = nullptr;
    bool open_ = false;
};

} // namespace btccw::node

#endif // BTCCW_HAS_SDR
#endif // BTCCW_NODE_SDR_INPUT_HPP
