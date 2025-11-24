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
    // --- SM5A ---
    if (ref_game == "AC_01") { // BALL
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "FL_02") { // Flagman
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "MT_03") { // Vermin
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "RC_04") { // Fire
        static const TimeAddress addr = {3,6, 3,7, 3,8, 3,9, 3,10, 3,11, 0}; return &addr; // from gnw_fire.py: 54-59
    } else if (ref_game == "IP_05" || ref_game == "IP_15") { // Judge
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "MH_06") { // Manhole
        static const TimeAddress addr = {0,10, 0,11, 0,12, 0,13, 0,14, 0,15, 8}; return &addr; // from gnw_manhole.py: 10-15
    } else if (ref_game == "CN_07" || ref_game == "CN_17") { // Helmet
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "LN_08") { // Lion
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "PR_21") { // Parachute
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "OC_22") { // Octopus
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "PP_23") { // Popeye
        static const TimeAddress addr = {3,6, 3,7, 3,8, 3,9, 3,10, 3,11, 8}; return &addr; // from gnw_popeye.py: 54-59
    } else if (ref_game == "FP_24") { // Chef
        static const TimeAddress addr = {2,4, 2,5, 2,6, 2,7, 2,8, 2,9, 2}; return &addr; // from gnw_chef.py: 36-41
    } else if (ref_game == "MC_25" || ref_game == "EG_26") { // Mickey Mouse / Egg
        static const TimeAddress addr = {2,4, 2,5, 2,6, 2,7, 2,8, 2,9, 2}; return &addr; // from gnw_egg.py: 36-41
    } else if (ref_game == "FR_27") { // Fire (Wide Screen)
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values

    // --- SM510 ---
    } else if (ref_game == "TL_28") { // Turtle Bridge
        static const TimeAddress addr = {1,4, 1,5, 1,6, 1,7, 1,8, 1,9, 8}; return &addr; // from gnw_tbridge.py: 20-25
    } else if (ref_game == "ID_29") { // Fire Attack
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "SP_30") { // Snoopy Tennis
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "OP_51") { // Oil Panic
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "DK_52") { // Donkey Kong
        static const TimeAddress addr = {4,4, 4,3, 4,2, 4,1, 4,6, 4,5, 2}; return &addr; // from gnw_dkong.py: 69-64
    } else if (ref_game == "DM_53") { // Mickey & Donald
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "GH_54") { // Green House
        static const TimeAddress addr = {1,4, 1,5, 1,6, 1,7, 1,8, 1,9, 8}; return &addr; 
    } else if (ref_game == "JR_55") { // Donkey Kong II
        static const TimeAddress addr = {4,4, 4,3, 4,2, 4,1, 4,6, 4,5, 2}; return &addr; 
    } else if (ref_game == "MW_56") { // Mario Bros
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "LP_57") { // Rain Shower
        static const TimeAddress addr = {1,4, 1,5, 1,6, 1,7, 1,8, 1,9, 8}; return &addr; 
    } else if (ref_game == "TC_58") { // Life Boat
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "MG_61") { // Squish
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "DJ_101") { // DK JR (Wide Screen)
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "ML_102") { // Mario's Cement Factory (Wide Screen)
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "NH_103") { // Manhole (Wide Screen)
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "TF_104") { // Tropical Fish
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "BU_201") { // Spitball Sparky
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "UD_202") { // Crab Grab
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "MG_8") { // Shuttle Voyage (Tronica)
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values

    // --- SM511/SM512 ---
    } else if (ref_game == "PB_59") { // Pinball
        static const TimeAddress addr = {0,5, 0,4, 0,3, 0,2, 0,1, 0,0, 2}; return &addr; // from gnw_bjack.py: 5-0
    } else if (ref_game == "BJ_60") { // Black jack
        static const TimeAddress addr = {0,5, 0,4, 0,3, 0,2, 0,1, 0,0, 2}; return &addr; // from gnw_bjack.py: 5-0
    } else if (ref_game == "BD_62") { // Bomb Sweeper
        static const TimeAddress addr = {1,0, 1,1, 1,2, 1,3, 1,4, 1,5, 2}; return &addr; // from gnw_bsweep.py: 16-21
    } else if (ref_game == "JB_63") { // Safe Buster
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "MV_64") { // Gold Cliff
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "ZL_65") { // Zelda
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "CM_72" || ref_game == "CM_72A") { // Mario's Cement Factory
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "SM_91") { // Snoopy (table top)
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "PG_92") { // Popeye (table top)
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "CJ_93") { // DK JR (Panorama)
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "TB_94") { // Mario's Bombs Away
        static const TimeAddress addr = {1,4, 1,5, 1,6, 1,7, 1,8, 1,9, 8}; return &addr; // from gnw_tbridge.py: 20-25 (placeholder)
    } else if (ref_game == "DC_95" || ref_game == "MK_96") { // Donkey Kong Circus / Mickey Mouse
        static const TimeAddress addr = {2,4, 2,5, 2,6, 2,7, 2,8, 2,9, 2}; return &addr; // from gnw_egg.py: 36-41 (placeholder)
    } else if (ref_game == "YM_801" || ref_game == "YM_105") { // Super Mario Bros
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "DR_802" || ref_game == "DR_106") { // Climber
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "BF_803" || ref_game == "BF_107") { // Balloon Fight
        static const TimeAddress addr = {2,3, 2,4, 2,5, 2,6, 2,7, 2,8, 2}; return &addr; // from gnw_bfightn.py: 35-34, 37-38, 31-30, 33-32
    } else if (ref_game == "MB_108") { // Mario The Juggler
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    } else if (ref_game == "BX_301" || ref_game == "AK_302" || ref_game == "HK_303") { // all vs g&w (3)
        static const TimeAddress addr = {4,8, 4,9, 4,10, 4,11, 4,12, 4,13, 0}; return &addr; // TODO: fill real values
    }
    return nullptr; // Return nullptr if no specific mapping exists
}

#endif // TIME_ADDRESSES_H
