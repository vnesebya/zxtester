#include "analyzer.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

analysis_result_t analyze_signal_buffer(const uint32_t *buffer, uint32_t word_count, double sample_rate) {
    analysis_result_t res = {0};
    uint32_t high_count = 0;
    uint32_t transitions = 0;
    uint32_t pulse_widths[2] = {0, 0};
    uint32_t pulse_counts[2] = {0, 0};
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
                pulse_counts[last_state]++;
                current_pulse_length = 1;
                last_state = current_state;
            } else {
                current_pulse_length++;
            }
        }
    }

    // add last pulse
    pulse_widths[last_state] += current_pulse_length;
    pulse_counts[last_state]++;

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

    // compute average pulse widths per pulse (in samples)
    if (pulse_counts[1] > 0) res.avg_high_pulse = (float)res.pulse_widths[1] / (float)pulse_counts[1];
    else res.avg_high_pulse = 0.0f;
    if (pulse_counts[0] > 0) res.avg_low_pulse = (float)res.pulse_widths[0] / (float)pulse_counts[0];
    else res.avg_low_pulse = 0.0f;

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

// Helper to get a single sample bit at global sample index
static inline uint8_t get_sample_bit(const uint32_t *buffer, uint32_t sample_index) {
    uint32_t word_idx = sample_index >> 5; // /32
    uint32_t bit = sample_index & 31;
    return (buffer[word_idx] >> bit) & 1u;
}

void reduce_buffer_to_32(const uint32_t *buffer, uint32_t word_count, reduce_t out[128], uint32_t avg_fullpulse_width) {
    uint8_t current_state;
    uint8_t current_pulse_length;
    uint8_t last_state = get_sample_bit(buffer, 0);
    uint8_t cursor = 0;  
    bool force_transition = false;
    bool first_sampleset = true;

    uint32_t offset = 1;
    
    for (; offset < word_count * 32; ++offset) {
        current_state = get_sample_bit(buffer, offset);
        if (last_state != current_state) {
            last_state = current_state;
            break;
        }
    }


    for (uint32_t i = offset; i < word_count * 32; i++) {
        current_state = get_sample_bit(buffer, i);

        if (force_transition || current_state != last_state) {
            if (current_pulse_length > avg_fullpulse_width / 2 ) {
                out[cursor++] = last_state;             
            } else {
                out[cursor++] = reduced_pin;
            }
            if (cursor == 128)
                return;

            current_pulse_length = 1;
            last_state = current_state;
        } else {
            current_pulse_length++;
        }
    }
}
