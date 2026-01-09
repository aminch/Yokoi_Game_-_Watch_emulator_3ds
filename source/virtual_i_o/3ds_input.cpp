#include "virtual_i_o/3ds_input.h"


bool Input_Manager_3ds::input_isHeld(u32 input){
    hidScanInput();
    last_input = input;
    return (hidKeysHeld()&input) == input;
};


bool Input_Manager_3ds::input_justPressed(u32 input, bool protect_bad){
    hidScanInput();
    bool result = (hidKeysDown()&input) == input;

    if(result){
        if(input != last_input){
            last_input = input;
            last_time_input = time_us_64_p();
            return true;
        }
        last_input = input;
        if(time_us_64_p() - last_time_input > PROTECT_BAD_INPUT
                    || time_us_64_p() < last_time_input // if time reset
                    || !protect_bad) 
        {
            last_time_input = time_us_64_p();
            return true;   
        }
    }
    return false;
}



bool Input_Manager_3ds::input_Held_Increase(u32 input){
    hidScanInput();
    bool result = (hidKeysHeld()&input) == input;

    if(result){
        if(input != last_input){
            last_input = input;
            last_time_input = time_us_64_p();
            return true;
        }
        last_input = input;
        if(time_us_64_p() - last_time_input > INCREASE_HELD
                    || time_us_64_p() < last_time_input ) // if time reset
        {
            last_time_input = time_us_64_p();
            return true;   
        }
    }
    return false;
}



