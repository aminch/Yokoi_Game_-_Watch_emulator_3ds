#pragma once

#include <cstdint>

// Initializes core state and ensures emulation thread is running.
void yokoi_frontend_init();

// Stops emulation thread and releases core resources.
void yokoi_frontend_shutdown();

// Notifies the renderer of a drawable size change.
void yokoi_frontend_resize(int width, int height);

// Renders combined view (panel = -1) or a specific panel (0/1).
void yokoi_frontend_render(int panel);

// Texture upload hooks (GL texture IDs are owned by the caller).
void yokoi_frontend_set_textures(
    uint32_t segmentTex, int segmentW, int segmentH,
    uint32_t backgroundTex, int backgroundW, int backgroundH,
    uint32_t consoleTex, int consoleW, int consoleH);

void yokoi_frontend_set_ui_texture(uint32_t uiTex, int uiW, int uiH);

// Returns the current texture generation (increments when selected game changes).
uint32_t yokoi_frontend_get_texture_generation();

// Audio pull API.
int yokoi_frontend_audio_get_sample_rate();
int yokoi_frontend_audio_read(int16_t* pcm, int frames);

// Controls which panel processes menu input when rendering split.
void yokoi_frontend_set_emulation_driver_panel(int panel);
