#ifndef ANALYZER_H
#define ANALYZER_H

#include <stdint.h>
#include <stdbool.h>

// signal type enumeration
typedef enum {
    SIGNAL_TYPE_UNKNOWN = 0,
    SIGNAL_TYPE_CONSTANT,
    SIGNAL_TYPE_PERFECT_SQUARE,
    SIGNAL_TYPE_PERIODIC
} signal_type_t;

// signal analyzer result structure
typedef struct {
    uint32_t high_count;
    uint32_t transitions;
    uint32_t pulse_widths[2]; // [0] low, [1] high (sum of lengths)
    uint32_t total_samples;
    double capture_duration_s;
    double estimated_freq;
    float avg_high_pulse;
    float avg_low_pulse;
    uint32_t first_words[10];
    uint32_t word_count;
    float duty_cycle; // duty cycle in percent (0.0 - 100.0)
    signal_type_t signal_type;
} analysis_result_t;

// Analyze buffer and return populated result
analysis_result_t analyze_signal_buffer(const uint32_t *buffer, uint32_t word_count, double sample_rate);

// Detect whether buffer contains any transitions (activity)
bool detect_signal_activity(const uint32_t *buffer, uint32_t word_count);

// Calculate duty cycle (percentage of HIGH samples) from the raw buffer
float calculate_duty_cycle(const uint32_t *buffer, uint32_t word_count);

// Reduce raw sample buffer to up to 32 logical samples (0/1).
// - `out` must point to a 32-byte array to receive 0/1 values.
// - Returns number of elements written into `out` (<=32).
// - Short spikes are removed by merging runs shorter than (median_run_length/10).
uint32_t reduce_buffer_to_32(const uint32_t *buffer, uint32_t word_count, uint8_t out[32]);

#endif // ANALYZER_H
