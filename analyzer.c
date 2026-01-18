#include "analyzer.h"
#include <stdint.h>
#include <stdbool.h>

analysis_result_t analyze_signal_buffer(const uint32_t *buffer, uint32_t word_count, double sample_rate) {
    analysis_result_t res = {0};
    uint32_t high_count = 0;
    uint32_t transitions = 0;
    uint32_t pulse_widths[2] = {0, 0};
    uint32_t current_pulse_length = 0;
    uint8_t last_state = (buffer[0] & 1);
    uint8_t current_state;

    for (uint32_t i = 0; i < word_count; i++) {
        uint32_t word = buffer[i];

        if (i < 10) res.first_words[i] = word;

        for (int bit = 0; bit < 32; bit++) {
            current_state = (word >> bit) & 1;

            if (current_state) high_count++;

            if (current_state != last_state) {
                transitions++;
                pulse_widths[last_state] += current_pulse_length;
                current_pulse_length = 1;
                last_state = current_state;
            } else {
                current_pulse_length++;
            }
        }
    }

    // add last pulse
    pulse_widths[last_state] += current_pulse_length;

    uint32_t total_samples = word_count * 32;

    res.high_count = high_count;
    res.transitions = transitions;
    res.pulse_widths[0] = pulse_widths[0];
    res.pulse_widths[1] = pulse_widths[1];
    res.total_samples = total_samples;
    res.word_count = word_count;
    // compute duty cycle (% of HIGH samples)
    if (total_samples > 0) {
        res.duty_cycle = ((float)high_count / (float)total_samples) * 100.0f;
    } else {
        res.duty_cycle = 0.0f;
    }

    if (transitions > 1) {
        res.capture_duration_s = (double)total_samples / sample_rate;
        res.estimated_freq = (transitions / 2.0) / res.capture_duration_s;
    } else {
        res.capture_duration_s = 0.0;
        res.estimated_freq = 0.0;
    }

    if (transitions > 1) {
        double denom = (transitions / 2.0);
        res.avg_high_pulse = (float)res.pulse_widths[1] / denom;
        res.avg_low_pulse = (float)res.pulse_widths[0] / denom;
    } else {
        res.avg_high_pulse = 0.0f;
        res.avg_low_pulse = 0.0f;
    }

    if (transitions == 0) res.signal_type = SIGNAL_TYPE_CONSTANT;
    else if (transitions == 2 && high_count == total_samples / 2) res.signal_type = SIGNAL_TYPE_PERFECT_SQUARE;
    else if (transitions >= 4) res.signal_type = SIGNAL_TYPE_PERIODIC;
    else res.signal_type = SIGNAL_TYPE_UNKNOWN;

    return res;
}

float calculate_duty_cycle(const uint32_t *buffer, uint32_t word_count) {
    if (!buffer || word_count == 0) return 0.0f;
    uint64_t high_count = 0;
    for (uint32_t i = 0; i < word_count; i++) {
        uint32_t w = buffer[i];
        // popcount for speed if available
#if defined(__GNUC__)
        high_count += __builtin_popcount(w);
#else
        for (int b = 0; b < 32; b++) {
            high_count += (w >> b) & 1u;
        }
#endif
    }
    uint64_t total = (uint64_t)word_count * 32ull;
    if (total == 0) return 0.0f;
    return ((float)high_count / (float)total) * 100.0f;
}

bool detect_signal_activity(const uint32_t *buffer, uint32_t word_count) {
    uint32_t check_words = word_count;
    uint8_t last_state = (buffer[0] & 1);

    for (uint32_t i = 0; i < check_words; i++) {
        uint32_t word = buffer[i];

        for (int bit = 0; bit < 32; bit++) {
            uint8_t current_state = (word >> bit) & 1;
            if (current_state != last_state) {
                return true;
            }
            last_state = current_state;
        }
    }
    return false;
}
