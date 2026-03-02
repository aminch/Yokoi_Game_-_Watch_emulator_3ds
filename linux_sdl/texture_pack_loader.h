#pragma once

#include <string>

#include <GLES3/gl3.h>

struct YokoiTextureSet {
    GLuint seg = 0;
    int seg_w = 0;
    int seg_h = 0;

    GLuint bg = 0;
    int bg_w = 0;
    int bg_h = 0;

    GLuint cs = 0;
    int cs_w = 0;
    int cs_h = 0;
};

// Loads the currently-selected game's textures from the already-loaded gw_pack.
// Returns true if at least one texture was successfully loaded.
bool yokoi_load_textures_from_pack(YokoiTextureSet& out, std::string* error_out);

// Deletes GL texture objects in the set (safe on zeros).
void yokoi_delete_textures(YokoiTextureSet& tex);
