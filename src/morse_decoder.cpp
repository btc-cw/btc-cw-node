#include "morse_decoder.hpp"

#include <btccw/morse.hpp>

namespace btccw::node {

MorseDecoder::MorseDecoder(int blocks_per_unit)
    : blocks_per_unit_(blocks_per_unit) {
    build_reverse_table();
}

void MorseDecoder::build_reverse_table() {
    // Build reverse table from MorseEncoder::lookup() for all supported chars.
    // Letters A-Z
    for (char c = 'A'; c <= 'Z'; ++c) {
        const char* pattern = btccw::MorseEncoder::lookup(c);
        if (pattern) {
            reverse_table_[pattern] = c;
        }
    }
    // Digits 0-9
    for (char c = '0'; c <= '9'; ++c) {
        const char* pattern = btccw::MorseEncoder::lookup(c);
        if (pattern) {
            reverse_table_[pattern] = c;
        }
    }
    // Punctuation used in Base43 charset
    for (char c : {'+', '/', '.', ':', '-', '?'}) {
        const char* pattern = btccw::MorseEncoder::lookup(c);
        if (pattern) {
            reverse_table_[pattern] = c;
        }
    }
    // Space is implicit (word gap), not in lookup table.
}

std::string MorseDecoder::decode(const std::vector<bool>& tones) const {
    if (tones.empty()) return {};

    // Convert boolean stream to run-length pairs: (is_on, length).
    struct Run {
        bool on;
        int  length;
    };
    std::vector<Run> runs;

    bool current = tones[0];
    int  count   = 1;
    for (std::size_t i = 1; i < tones.size(); ++i) {
        if (tones[i] == current) {
            ++count;
        } else {
            runs.push_back({current, count});
            current = tones[i];
            count = 1;
        }
    }
    runs.push_back({current, count});

    // Timing thresholds (in blocks):
    //   dot vs dash boundary:        2 * blocks_per_unit
    //   intra-char vs inter-char:    2 * blocks_per_unit
    //   inter-char vs word gap:      5 * blocks_per_unit
    int dot_dash_threshold = 2 * blocks_per_unit_;
    int word_gap_threshold = 5 * blocks_per_unit_;

    std::string result;
    std::string current_pattern; // accumulates dots/dashes for one character

    for (const auto& run : runs) {
        if (run.on) {
            // ON run: classify as dot or dash.
            if (run.length < dot_dash_threshold) {
                current_pattern += '.';
            } else {
                current_pattern += '-';
            }
        } else {
            // OFF run: classify gap type.
            if (run.length < dot_dash_threshold) {
                // Intra-character gap — do nothing, elements accumulate.
            } else if (run.length < word_gap_threshold) {
                // Inter-character gap — flush current character.
                if (!current_pattern.empty()) {
                    auto it = reverse_table_.find(current_pattern);
                    if (it != reverse_table_.end()) {
                        result += it->second;
                    } else {
                        result += '?'; // unknown pattern
                    }
                    current_pattern.clear();
                }
            } else {
                // Word gap — flush character then add space.
                if (!current_pattern.empty()) {
                    auto it = reverse_table_.find(current_pattern);
                    if (it != reverse_table_.end()) {
                        result += it->second;
                    } else {
                        result += '?';
                    }
                    current_pattern.clear();
                }
                result += ' ';
            }
        }
    }

    // Flush any remaining pattern.
    if (!current_pattern.empty()) {
        auto it = reverse_table_.find(current_pattern);
        if (it != reverse_table_.end()) {
            result += it->second;
        } else {
            result += '?';
        }
    }

    return result;
}

} // namespace btccw::node
