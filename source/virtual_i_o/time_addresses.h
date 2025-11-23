#ifndef TIME_ADDRESSES_H
#define TIME_ADDRESSES_H
#include <stdint.h>
#include <string>


// Structure for time addresses (column and line for each digit)
struct TimeAddress {
    uint8_t col_hour_tens,    line_hour_tens;
    uint8_t col_hour_units,   line_hour_units;
    uint8_t col_minute_tens,  line_minute_tens;
    uint8_t col_minute_units, line_minute_units;
    uint8_t col_second_tens,  line_second_tens;
    uint8_t col_second_units, line_second_units;
    uint8_t pm_bit;                                 // PM bit for 12-hour format
};

inline const TimeAddress* get_time_addresses(const std::string& ref_game) {
    // Example mapping for Donkey Kong II (JR-55)
    if (ref_game == "JR_55") {
        // Example: fill with actual (col, line) for each digit for Donkey Kong II
        static const TimeAddress dk2_time_addr = {
            4, 4, // hour tens: col 4, line 4
            4, 3, // hour units: col 4, line 3
            4, 2, // minute tens: col 4, line 2
            4, 1, // minute units: col 4, line 1
            4, 6, // second tens: col 4, line 6
            4, 5, // second units: col 4, line 5
            2     // pm bit
        };
        return &dk2_time_addr;
    }

    return nullptr; // Return nullptr if no specific mapping exists
}

#endif // TIME_ADDRESSES_H
