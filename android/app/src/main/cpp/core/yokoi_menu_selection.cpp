#include "yokoi_menu_selection.h"

#include <android/log.h>

#include "std/GW_ROM.h"
#include "std/load_file.h"
#include "std/platform_paths.h"
#include "std/settings.h"

#include "yokoi_layout.h"
#include "yokoi_runtime_state.h"

namespace {
constexpr const char* kLogTag = "Yokoi";
}

void yokoi_menu_select_game_by_index(uint8_t idx) {
    g_game_index = idx;
    g_game = load_game(idx);
    if (!g_game) {
        __android_log_write(ANDROID_LOG_ERROR, kLogTag, "menu_select_game: load_game failed");
        return;
    }
    save_last_game(get_name(idx));
    rebuild_layout_for_menu();
    g_texture_generation.fetch_add(1);
}
