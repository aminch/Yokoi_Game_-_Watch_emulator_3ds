#include "texture_pack_loader.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include "png_decode.h"

#include "std/gw_pack.h"

// Access currently selected game.
#include "yokoi_runtime_state.h"

namespace {

static std::string t3x_path_to_png_basename(const std::string& p) {
    if (p.empty()) return std::string();
    std::string base = p;
    size_t slash = base.find_last_of('/');
    if (slash != std::string::npos) {
        base = base.substr(slash + 1);
    }
    const std::string t3x = ".t3x";
    if (base.size() >= t3x.size() && base.compare(base.size() - t3x.size(), t3x.size(), t3x) == 0) {
        base.replace(base.size() - t3x.size(), t3x.size(), ".png");
    }
    return base;
}

static GLuint create_gl_texture_from_rgba(const uint8_t* rgba, int w, int h) {
    if (!rgba || w <= 0 || h <= 0) {
        return 0;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

static bool load_one_png_from_pack(const std::string& name, GLuint& out_tex, int& out_w, int& out_h, std::string* error_out) {
    out_tex = 0;
    out_w = 0;
    out_h = 0;

    if (name.empty()) {
        if (error_out) *error_out = "empty texture name";
        return false;
    }

    const uint8_t* bytes = nullptr;
    size_t size = 0;
    if (!gw_pack::get_file_bytes(name, bytes, size) || !bytes || size == 0) {
        if (error_out) *error_out = "missing in pack: " + name;
        return false;
    }

    int w = 0;
    int h = 0;
    std::vector<uint8_t> rgba;
    std::string dec_err;
    if (!png_decode_rgba(bytes, size, w, h, rgba, &dec_err)) {
        if (error_out) *error_out = "png decode failed: " + name + (dec_err.empty() ? "" : (" (" + dec_err + ")"));
        return false;
    }

    out_tex = create_gl_texture_from_rgba(rgba.data(), w, h);
    out_w = w;
    out_h = h;

    if (out_tex == 0) {
        if (error_out) *error_out = "gl texture upload failed: " + name;
        return false;
    }
    return true;
}

} // namespace

bool yokoi_load_textures_from_pack(YokoiTextureSet& out, std::string* error_out) {
    if (!g_game) {
        if (error_out) *error_out = "no selected game";
        return false;
    }
    if (!gw_pack::is_loaded()) {
        if (error_out) *error_out = "rom pack not loaded";
        return false;
    }

    const std::string seg = t3x_path_to_png_basename(g_game->path_segment);
    const std::string bg = t3x_path_to_png_basename(g_game->path_background);
    const std::string cs = t3x_path_to_png_basename(g_game->path_console);

    YokoiTextureSet tmp;
    std::string err;
    bool any = false;

    if (load_one_png_from_pack(seg, tmp.seg, tmp.seg_w, tmp.seg_h, &err)) {
        any = true;
    } else if (error_out && !seg.empty()) {
        *error_out = err;
    }

    if (load_one_png_from_pack(bg, tmp.bg, tmp.bg_w, tmp.bg_h, &err)) {
        any = true;
    } else if (error_out && error_out->empty() && !bg.empty()) {
        *error_out = err;
    }

    if (load_one_png_from_pack(cs, tmp.cs, tmp.cs_w, tmp.cs_h, &err)) {
        any = true;
    } else if (error_out && error_out->empty() && !cs.empty()) {
        *error_out = err;
    }

    if (!any) {
        yokoi_delete_textures(tmp);
        return false;
    }

    // Success: replace existing.
    yokoi_delete_textures(out);
    out = tmp;
    return true;
}

void yokoi_delete_textures(YokoiTextureSet& tex) {
    if (tex.seg) glDeleteTextures(1, &tex.seg);
    if (tex.bg) glDeleteTextures(1, &tex.bg);
    if (tex.cs) glDeleteTextures(1, &tex.cs);
    tex = YokoiTextureSet{};
}
