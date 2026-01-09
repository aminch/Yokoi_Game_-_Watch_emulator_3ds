#include <3ds.h>
#include <cstdint>
#include "virtual_i_o/virtual_input.h"
#include "std/timer.h"


constexpr uint64_t PROTECT_BAD_INPUT = 5* 1000ULL; // in us
constexpr uint64_t INCREASE_HELD = 40* 1000ULL; // in us



class Input_Manager_3ds {

    public:

    private:
        u32 last_input = 0;
        uint64_t last_time_input = 0;


    public: 
        bool input_justPressed(u32 input, bool protect_bad = true);
        bool input_isHeld(u32 input);
        bool input_Held_Increase(u32 input);
    

};
