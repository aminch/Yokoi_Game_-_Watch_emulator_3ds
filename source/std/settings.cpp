#include "settings.h"
#include "load_file.h"
#include "platform_paths.h"
#include <stdio.h>
#include <string.h>

// Global settings instance
AppSettings g_settings;

static std::string settings_file_path() {
    return storage_path("yokoi_gw_settings.dat");
}

void load_settings() {
    std::string path = settings_file_path();
    FILE* file = fopen(path.c_str(), "rb");
    if (file) {
        size_t read = fread(&g_settings, sizeof(AppSettings), 1, file);
        fclose(file);
        
        if (read != 1) {
            // File corrupted, reset to defaults
            reset_settings_to_default();
        }
    } else {
        // No settings file, use defaults
        reset_settings_to_default();
    }
}

void save_settings() {
    std::string path = settings_file_path();
    FILE* file = fopen(path.c_str(), "wb");
    if (file) {
        fwrite(&g_settings, sizeof(AppSettings), 1, file);
        fclose(file);
    }
}

void reset_settings_to_default() {
    g_settings = AppSettings(); // Reset to default constructor values
}

// Save the last selected game name
void save_last_game(const std::string& game_name) {
    strncpy(g_settings.last_game_name, game_name.c_str(), sizeof(g_settings.last_game_name) - 1);
    g_settings.last_game_name[sizeof(g_settings.last_game_name) - 1] = '\0'; // Ensure null termination
    save_settings();
}

// Load the last game index by matching the saved name
uint8_t load_last_game_index() {
    if (g_settings.last_game_name[0] == '\0') {
        return 0; // No saved game, return first game
    }
    
    // Search for the game with matching name
    size_t num_games = get_nb_name();
    for (uint8_t i = 0; i < num_games; i++) {
        if (get_name(i) == g_settings.last_game_name) {
            return i;
        }
    }
    
    return 0; // If not found, return first game
}
