#include "analyzer.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

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

// Helper to get a single sample bit at global sample index
static inline uint8_t get_sample_bit(const uint32_t *buffer, uint32_t sample_index) {
    uint32_t word_idx = sample_index >> 5; // /32
    uint32_t bit = sample_index & 31;
    return (buffer[word_idx] >> bit) & 1u;
}

uint32_t reduce_buffer_to_32(const uint32_t *buffer, uint32_t word_count, uint8_t out[32]) {
    if (!buffer || word_count == 0 || !out) return 0;

    uint32_t total_samples = word_count * 32u;
    if (total_samples == 0) return 0;

    // count transitions across whole buffer
    uint8_t last = get_sample_bit(buffer, 0);
    uint32_t transitions = 0;
    for (uint32_t i = 1; i < total_samples; i++) {
        uint8_t cur = get_sample_bit(buffer, i);
        if (cur != last) {
            transitions++;
            last = cur;
        }
    }

    if (transitions == 0) {
        // constant signal, single value
        out[0] = get_sample_bit(buffer, 0);
        return 1;
    }

    // compute spike threshold: runs shorter than this (in samples) are considered spikes
    // defined as ~period/10. Using transitions we derive samples_per_period = (2*total_samples)/transitions
    // so spike_thresh = samples_per_period/10 = total_samples/(5*transitions)
    uint32_t spike_thresh = (transitions > 0) ? (total_samples / (5u * transitions)) : 1u;
    if (spike_thresh < 1) spike_thresh = 1u;

    // Build run-length list
    // dynamic array via malloc, grow as needed
    typedef struct { uint8_t v; uint32_t len; } run_t;
    uint32_t cap = 256;
    run_t *runs = (run_t *)malloc(sizeof(run_t) * cap);
    if (!runs) return 0;
    uint32_t runs_count = 0;

    uint32_t idx = 0;
    uint8_t curv = get_sample_bit(buffer, 0);
    uint32_t curlen = 1;
    for (idx = 1; idx < total_samples; idx++) {
        uint8_t b = get_sample_bit(buffer, idx);
        if (b == curv) curlen++;
        else {
            if (runs_count + 1 >= cap) {
                uint32_t ncap = cap * 2;
                run_t *nr = (run_t *)realloc(runs, sizeof(run_t) * ncap);
                if (!nr) break; // out of memory, stop building
                runs = nr; cap = ncap;
            }
            runs[runs_count].v = curv;
            runs[runs_count].len = curlen;
            runs_count++;
            curv = b;
            curlen = 1;
        }
    }
    // append last run
    if (runs_count + 1 >= cap) {
        run_t *nr = (run_t *)realloc(runs, sizeof(run_t) * (cap + 1));
        if (nr) { runs = nr; cap = cap + 1; }
    }
    runs[runs_count].v = curv;
    runs[runs_count].len = curlen;
    runs_count++;

    // Merge short runs (spikes) into largest neighbor
    bool changed = true;
    while (changed) {
        changed = false;
        if (runs_count <= 1) break;
        for (uint32_t i = 0; i < runs_count; i++) {
            if (runs[i].len < spike_thresh) {
                if (i == 0) {
                    // merge into next
                    runs[1].len += runs[0].len;
                    // remove 0
                    memmove(&runs[0], &runs[1], sizeof(run_t) * (runs_count - 1));
                    runs_count--;
                } else if (i == runs_count - 1) {
                    // merge into previous
                    runs[i-1].len += runs[i].len;
                    runs_count--;
                } else {
                    // merge into larger neighbor
                    if (runs[i-1].len >= runs[i+1].len) {
                        runs[i-1].len += runs[i].len;
                        // remove runs[i]
                        memmove(&runs[i], &runs[i+1], sizeof(run_t) * (runs_count - i - 1));
                        runs_count--;
                    } else {
                        runs[i+1].len += runs[i].len;
                        memmove(&runs[i], &runs[i+1], sizeof(run_t) * (runs_count - i - 1));
                        runs_count--;
                    }
                }
                changed = true;
                break; // restart scan
            }
        }
    }

    // fill output with first up to 32 run values
    uint32_t out_count = (runs_count < 32u) ? runs_count : 32u;
    for (uint32_t i = 0; i < out_count; i++) out[i] = runs[i].v;

    free(runs);
    return out_count;
}
