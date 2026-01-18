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
    signal_type_t signal_type;
} analysis_result_t;

// Analyze buffer and return populated result
analysis_result_t analyze_signal_buffer(const uint32_t *buffer, uint32_t word_count, double sample_rate);

// Detect whether buffer contains any transitions (activity)
bool detect_signal_activity(const uint32_t *buffer, uint32_t word_count);

#endif // ANALYZER_H
