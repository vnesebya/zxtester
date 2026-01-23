#ifndef Units_h
#define Units

void printFreq(char *s, long frequency_hz){

        // 0 HZ
        if (frequency_hz < 1.0f) {
        sprintf(s, "<1 Hz");
        // 0.999 - 9.99 HZ
        } else if (frequency_hz < 10.0f) {
        sprintf(s, "%.3f Hz", frequency_hz / 1.0);
        // 10.00 - 99.99 HZ
        } else if (frequency_hz < 100.0f) {
        sprintf(s, "%.2f Hz", frequency_hz / 1.0);
        // 100.0 - 999.9 HZ
        } else if (frequency_hz < 1000.0f) {
        sprintf(s, "%.1f Hz", frequency_hz / 1.0);

        // 1.000 - 9.999 KHz
        } else if (frequency_hz < 10000.0f) {
        sprintf(s, "%.3f KHz", frequency_hz / 1000.0);
        // 10.00 - 99.99 KHz
        } else if (frequency_hz < 100000.0f) {
        sprintf(s, "%.2f KHz", frequency_hz / 1000.0);
        // 100.1 - 999.9 KHz
        } else if (frequency_hz < 1000000.0f) {
        sprintf(s, "%.1f KHz", frequency_hz / 1000.0);

        // 1.000 - 9.999 MHz
        } else if (frequency_hz < 10000000.0f) {
        sprintf(s, "%.3f MHz", frequency_hz / 1000000.0);
        // 10.00 - 99.99 MHz
        } else if (frequency_hz < 100000000.0f) {
        sprintf(s, "%.2f MHz", frequency_hz / 1000000.0);
        // 100.1 - 999.9 MHz
        } else if (frequency_hz < 1000000000.0f) {
        sprintf(s, "%.1f MHz", frequency_hz / 1000000.0);
        // > 1 GHz 
        } else {
        sprintf(s, ">1 GHz");
        }

}
#endif