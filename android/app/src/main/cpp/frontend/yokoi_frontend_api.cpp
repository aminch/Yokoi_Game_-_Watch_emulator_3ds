#include "yokoi_frontend_api.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "std/GW_ROM.h"
#include "std/gw_pack.h"
#include "std/platform_paths.h"
#include "std/settings.h"
#include "std/savestate.h"

#include "virtual_i_o/virtual_input.h"

#include "yokoi_app_flow.h"
#include "yokoi_app_modes.h"
#include "yokoi_audio.h"
#include "yokoi_controller_state.h"
#include "yokoi_cpu_utils.h"
#include "yokoi_emulation_thread.h"
#include "yokoi_game_loader.h"
#include "yokoi_gl.h"
#include "yokoi_layout.h"
#include "yokoi_menu_selection.h"
#include "yokoi_runtime_state.h"
#include "yokoi_segments_state.h"

namespace {

constexpr bool kRenderConsoleOverlay = true;

// Verified-correct PNG atlas mapping for this project:
// - UV origin behaves like "bottom-left", but V must be flipped for OpenGL sampling.
// - Atlas is mirrored on X and must be flipped horizontally.
constexpr bool kUvFlipX = true;
constexpr bool kUvFlipV = true;

static GlResources* get_or_create_gl_for_current_context() {
    EGLContext ctx = eglGetCurrentContext();
    if (ctx == EGL_NO_CONTEXT) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(g_gl_mutex);
    for (auto& r : g_gl) {
        if (r.ctx == ctx) {
            return &r;
        }
    }

    GlResources r;
    r.ctx = ctx;
    yokoi_gl_init_resources(r);
    g_gl.push_back(r);
    return &g_gl.back();
}

static void append_quad_ndc_uv(std::vector<RenderVertex>& out,
                               float x, float y, float w, float h,
                               float u0, float v0, float u1, float v1) {
    float left = (x / g_canvas_w) * 2.0f - 1.0f;
    float right = ((x + w) / g_canvas_w) * 2.0f - 1.0f;
    float top = 1.0f - (y / g_canvas_h) * 2.0f;
    float bottom = 1.0f - ((y + h) / g_canvas_h) * 2.0f;

    out.push_back({left, bottom, u0, v0});
    out.push_back({right, bottom, u1, v0});
    out.push_back({right, top, u1, v1});

    out.push_back({right, top, u1, v1});
    out.push_back({left, top, u0, v1});
    out.push_back({left, bottom, u0, v0});
}

static void append_quad_ndc_uv_canvas(std::vector<RenderVertex>& out,
                                      float canvas_w, float canvas_h,
                                      float x, float y, float w, float h,
                                      float u0, float v0, float u1, float v1) {
    if (canvas_w <= 0.0f) canvas_w = 1.0f;
    if (canvas_h <= 0.0f) canvas_h = 1.0f;

    float left = (x / canvas_w) * 2.0f - 1.0f;
    float right = ((x + w) / canvas_w) * 2.0f - 1.0f;
    float top = 1.0f - (y / canvas_h) * 2.0f;
    float bottom = 1.0f - ((y + h) / canvas_h) * 2.0f;

    out.push_back({left, bottom, u0, v0});
    out.push_back({right, bottom, u1, v0});
    out.push_back({right, top, u1, v1});

    out.push_back({right, top, u1, v1});
    out.push_back({left, top, u0, v1});
    out.push_back({left, bottom, u0, v0});
}

static void calc_uv_rect(float texW, float texH,
                         float u, float v, float w, float h,
                         float& out_u0, float& out_v0, float& out_u1, float& out_v1) {
    if (texW <= 0.0f) texW = 1.0f;
    if (texH <= 0.0f) texH = 1.0f;

    out_u0 = u / texW;
    out_u1 = (u + w) / texW;
    if (kUvFlipX) {
        float tmp = out_u0;
        out_u0 = out_u1;
        out_u1 = tmp;
    }

    if (kUvFlipV) {
        out_v0 = 1.0f - (v / texH);
        out_v1 = 1.0f - ((v + h) / texH);
    } else {
        out_v0 = v / texH;
        out_v1 = (v + h) / texH;
    }
}

static void render_frame(GlResources& r, int panel) {
    // Always clear to black so letterbox/pillarbox areas are black.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (r.program == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_render_mutex);

    const int mode = g_app_mode.load();
    const bool is_menu = (mode != MODE_GAME);

    const bool is_combined = (panel < 0);
    const bool is_panel0 = (panel == 0);
    const bool is_panel1 = (panel == 1);

    const int driver = g_emulation_driver_panel.load();
    const bool is_driver_panel = (driver == 0 && is_panel0) || (driver == 1 && is_panel1);

    // Only the combined renderer or the configured driver panel handles menu input.
    if (is_combined || is_driver_panel) {
        static uint32_t prev_ctl_mask = 0;
        uint32_t ctl_mask = yokoi_controller_get_mask();
        uint32_t ctl_down = ctl_mask & ~prev_ctl_mask;
        prev_ctl_mask = ctl_mask;

        // Apply queued menu navigation on the GL thread (avoids races with the renderer).
        // In menu mode, this changes the selection. In game mode, ignore.
        auto get_mfr = [&](uint8_t idx) -> uint8_t {
            const GW_rom* g = load_game(idx);
            return g ? g->manufacturer : GW_rom::MANUFACTURER_NINTENDO;
        };

        auto wrap_index = [&](int i, int n) -> uint8_t {
            while (i < 0) i += n;
            i = i % n;
            return (uint8_t)i;
        };

        auto select_next_with_mfr = [&](int dir, uint8_t want_mfr) {
            const int n = (int)get_nb_name();
            if (n <= 0) return;
            const int start = (int)g_game_index;
            for (int step = 0; step < n; step++) {
                const uint8_t cand = wrap_index(start + dir * (step + 1), n);
                if (get_mfr(cand) == want_mfr) {
                    yokoi_menu_select_game_by_index(cand);
                    return;
                }
            }
        };

        const int mfr_count = (int)GW_rom::MANUFACTURER_COUNT;
        std::vector<uint8_t> has_mfr(mfr_count, 0);
        {
            const int n = (int)get_nb_name();
            for (uint8_t i = 0; i < (uint8_t)n; i++) {
                const uint8_t m = get_mfr(i);
                if (m < GW_rom::MANUFACTURER_COUNT) {
                    has_mfr[(int)m] = 1;
                }
            }
        }

        auto next_available_mfr = [&](uint8_t cur, int dir, uint8_t& out_mfr) -> bool {
            if (mfr_count <= 1) {
                return false;
            }
            if (cur >= GW_rom::MANUFACTURER_COUNT) {
                cur = GW_rom::MANUFACTURER_NINTENDO;
            }
            // Try at most MANUFACTURER_COUNT candidates to avoid infinite loops.
            for (int step = 1; step <= mfr_count; step++) {
                int cand = (int)cur + (dir * step);
                cand %= mfr_count;
                if (cand < 0) {
                    cand += mfr_count;
                }
                if (has_mfr[cand]) {
                    out_mfr = (uint8_t)cand;
                    return true;
                }
            }
            return false;
        };

        auto select_next_other_mfr = [&](int dir) {
            const int n = (int)get_nb_name();
            if (n <= 0) return;
            const uint8_t cur_mfr = get_mfr(g_game_index);
            uint8_t new_mfr = cur_mfr;
            if (!next_available_mfr(cur_mfr, dir, new_mfr) || new_mfr == cur_mfr) {
                return;
            }

            uint8_t saved_idx = 0;
            if (yokoi_menu_try_get_last_index_for_manufacturer(new_mfr, &saved_idx)) {
                yokoi_menu_select_game_by_index(saved_idx);
                return;
            }

            // No saved selection yet: pick the nearest game in the requested direction.
            select_next_with_mfr(dir, new_mfr);
        };

        const int mfr_delta = g_pending_manufacturer_delta.exchange(0);
        const int game_delta = g_pending_game_delta.exchange(0);
        if (mode == MODE_MENU_SELECT) {
            if (mfr_delta != 0) {
                const int dir = (mfr_delta > 0) ? +1 : -1;
                const int steps = (mfr_delta > 0) ? mfr_delta : -mfr_delta;
                for (int i = 0; i < steps; i++) {
                    select_next_other_mfr(dir);
                }
            }
            if (game_delta != 0) {
                const int dir = (game_delta > 0) ? +1 : -1;
                const int steps = (game_delta > 0) ? game_delta : -game_delta;
                const uint8_t want = get_mfr(g_game_index);
                for (int i = 0; i < steps; i++) {
                    select_next_with_mfr(dir, want);
                }
            }
        }

        // Controller behavior in menu.
        if (mode == MODE_MENU_SELECT) {
            if (ctl_down & CTL_DPAD_RIGHT) {
                g_pending_game_delta.fetch_add(1);
            }
            if (ctl_down & CTL_DPAD_LEFT) {
                g_pending_game_delta.fetch_sub(1);
            }
            if (ctl_down & CTL_DPAD_UP) {
                g_pending_manufacturer_delta.fetch_add(1);
            }
            if (ctl_down & CTL_DPAD_DOWN) {
                g_pending_manufacturer_delta.fetch_sub(1);
            }
            if (ctl_down & CTL_A) {
                g_menu_load_choice.store(yokoi_get_default_menu_load_choice_for_game(g_game_index));
                g_app_mode.store(MODE_MENU_LOAD_PROMPT);
            }
        } else if (mode == MODE_MENU_LOAD_PROMPT) {
            if (ctl_down & (CTL_DPAD_LEFT | CTL_DPAD_RIGHT)) {
                int choice = g_menu_load_choice.load();
                choice = (choice == 0) ? 1 : 0;
                // If no savestate exists, force choice to fresh.
                if (choice == 1 && !save_state_exists(g_game_index)) {
                    choice = 0;
                }
                g_menu_load_choice.store(choice);
            }
            if (ctl_down & CTL_B) {
                g_app_mode.store(MODE_MENU_SELECT);
            }
            if (ctl_down & CTL_A) {
                const bool want_load = (g_menu_load_choice.load() != 0) && save_state_exists(g_game_index);
                yokoi_start_game_from_menu(want_load);
            }
        }

        // Game mode stepping is handled by the dedicated emulation thread.
    }

    glUseProgram(r.program);
    glBindVertexArray(r.vao);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Default shader mode: multiply sampled RGBA by uMul (legacy behavior).
    if (r.uAlphaOnly >= 0) {
        glUniform1f(r.uAlphaOnly, 0.0f);
    }

    auto draw_vertices = [&](GLuint tex, const std::vector<RenderVertex>& verts) {
        if (tex == 0 || verts.empty()) return;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(r.uTex, 0);
        glBindBuffer(GL_ARRAY_BUFFER, r.vbo);
        const size_t bytes = verts.size() * sizeof(RenderVertex);
        if (bytes > r.vbo_capacity_bytes) {
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, verts.data(), GL_DYNAMIC_DRAW);
            r.vbo_capacity_bytes = bytes;
        } else {
            glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)bytes, verts.data());
        }
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)verts.size());
    };

    // Determine which slice of the combined canvas we are drawing.
    float panel_x = 0.0f;
    float panel_y = 0.0f;
    float panel_w = g_combined_canvas_w;
    float panel_h = g_combined_canvas_h;

    if (!is_combined) {
        if (is_panel0) {
            panel_x = g_top_off_x;
            panel_y = 0.0f;
            panel_w = g_top_canvas_w;
            panel_h = g_top_canvas_h;
        } else if (is_panel1) {
            panel_x = g_bottom_off_x;
            panel_y = g_bottom_off_y;
            panel_w = g_bottom_canvas_w;
            panel_h = g_bottom_canvas_h;
        }
    }

    if (panel_w <= 0.0f) panel_w = 1.0f;
    if (panel_h <= 0.0f) panel_h = 1.0f;

    float contentW = panel_w;
    float contentH = panel_h;
    float contentAspect = contentW / contentH;
    float viewAspect = 1.0f;
    if (r.width > 0 && r.height > 0) {
        viewAspect = (float)r.width / (float)r.height;
    }

    float sx = 1.0f;
    float sy = 1.0f;
    if (viewAspect > contentAspect) {
        sx = contentAspect / viewAspect;
    } else {
        sy = viewAspect / contentAspect;
    }
    glUniform2f(r.uScale, sx, sy);

    // In portrait, top-align vertically letterboxed content so the unused space is at the bottom.
    // This helps keep the bottom area clearer for touch controls.
    if (r.uOffset >= 0) {
        float ox = 0.0f;
        float oy = 0.0f;
        if (r.height > r.width && sy < 1.0f) {
            oy = 1.0f - sy;
        }
        glUniform2f(r.uOffset, ox, oy);
    }

    auto to_local_x = [&](float gx) { return gx - panel_x; };
    auto to_local_y = [&](float gy) { return gy - panel_y; };

    auto get_screen_base_global = [&](uint8_t screen, float& outX, float& outY) {
        if (g_split_two_screens_to_panels) {
            if (screen == 0) {
                outX = g_top_off_x;
                outY = 0.0f;
            } else {
                outX = g_bottom_off_x;
                outY = g_bottom_off_y;
            }
        } else {
            outX = g_top_off_x + g_screen_off_x[screen];
            outY = g_screen_off_y[screen];
        }
    };

    // Match 3DS: for mask segment atlases, the "fond" color is the classic dark segment color.
    constexpr uint32_t kMaskFondRgb = 0x080908;
    const bool is_mask_game = (!is_menu && ((g_segment_info[3] & 0x01) != 0));
    uint32_t bg = is_mask_game ? kMaskFondRgb : g_settings.background_color;
    float br = ((bg >> 16) & 0xFF) / 255.0f;
    float bgc = ((bg >> 8) & 0xFF) / 255.0f;
    float bb = (bg & 0xFF) / 255.0f;

    const bool panel_is_game = (!is_menu) && (is_combined || is_panel0 || (g_split_two_screens_to_panels && is_panel1));

    // Menu UI layer (top panel): Java-provided texture.
    if (is_menu && r.tex_ui != 0 && r.tex_ui_w > 0 && r.tex_ui_h > 0) {
        const bool want_ui = is_combined || is_panel0;
        if (want_ui) {
            glUniform4f(r.uMul, 1.0f, 1.0f, 1.0f, 1.0f);
            static thread_local std::vector<RenderVertex> ui_verts;
            ui_verts.clear();
            ui_verts.reserve(6);

            float u0 = 0.0f, u1 = 1.0f;
            float v0 = 0.0f, v1 = 1.0f;
            if (kUvFlipV) {
                v0 = 1.0f;
                v1 = 0.0f;
            }

            float dx = 0.0f;
            float dy = 0.0f;
            float dw = g_top_canvas_w;
            float dh = g_top_canvas_h;
            if (is_combined) {
                dx = to_local_x(g_top_off_x);
                dy = to_local_y(0.0f);
            }
            append_quad_ndc_uv_canvas(ui_verts, contentW, contentH, dx, dy, dw, dh, u0, v0, u1, v1);
            draw_vertices(r.tex_ui, ui_verts);
        }
    }

    if (panel_is_game && r.tex_white != 0) {
        if (r.uAlphaOnly >= 0) {
            glUniform1f(r.uAlphaOnly, 0.0f);
        }
        glUniform4f(r.uMul, br, bgc, bb, 1.0f);
        static thread_local std::vector<RenderVertex> fill_verts;
        fill_verts.clear();
        fill_verts.reserve(6);
        append_quad_ndc_uv_canvas(fill_verts, contentW, contentH, 0.0f, 0.0f, contentW, contentH, 0.0f, 0.0f, 1.0f, 1.0f);
        draw_vertices(r.tex_white, fill_verts);
    }

    // Background layer.
    if (panel_is_game && r.tex_background != 0 && g_game && g_game->background_info && r.tex_background_w > 0 && r.tex_background_h > 0) {
        const uint16_t* bi = g_game->background_info;
        const bool want_bg_shadow = (bi[(size_t)(2 + g_nb_screen * 4)] == 1);

        if (want_bg_shadow) {
            if (r.uAlphaOnly >= 0) {
                glUniform1f(r.uAlphaOnly, 0.0f);
            }
            glUniform4f(r.uMul, 0.0f, 0.0f, 0.0f, (float)0x24 / 255.0f);
            static thread_local std::vector<RenderVertex> bg_shadow_verts;
            bg_shadow_verts.clear();
            bg_shadow_verts.reserve(g_nb_screen * 6);

            float texW = (float)r.tex_background_w;
            float texH = (float)r.tex_background_h;

            for (uint8_t screen = 0; screen < g_nb_screen; screen++) {
                if (!is_combined) {
                    if (!g_split_two_screens_to_panels && is_panel1) {
                        continue;
                    }
                    if (g_split_two_screens_to_panels && (int)screen != panel) {
                        continue;
                    }
                }

                uint16_t id = (uint16_t)(2 + screen * 4);
                float u = (float)bi[id + 0];
                float v = (float)bi[id + 1];
                float w = (float)bi[id + 2];
                float h = (float)bi[id + 3];

                float u0, v0, u1, v1;
                calc_uv_rect(texW, texH, u, v, w, h, u0, v0, u1, v1);

                uint16_t scale = g_segment_info[2] ? g_segment_info[2] : 1;
                float dw = (float)g_segment_info[4 + screen * 2] / (float)scale;
                float dh = (float)g_segment_info[5 + screen * 2] / (float)scale;

                float gx = 0.0f;
                float gy = 0.0f;
                get_screen_base_global(screen, gx, gy);
                float dx = to_local_x(gx) + 6.0f;
                float dy = to_local_y(gy) + 6.0f;

                append_quad_ndc_uv_canvas(bg_shadow_verts, contentW, contentH, dx, dy, dw, dh, u0, v0, u1, v1);
            }

            draw_vertices(r.tex_background, bg_shadow_verts);
        }

        if (r.uAlphaOnly >= 0) {
            glUniform1f(r.uAlphaOnly, 0.0f);
        }
        glUniform4f(r.uMul, 1.0f, 1.0f, 1.0f, 1.0f);
        static thread_local std::vector<RenderVertex> bg_verts;
        bg_verts.clear();
        bg_verts.reserve(g_nb_screen * 6);
        float texW = (float)r.tex_background_w;
        float texH = (float)r.tex_background_h;

        for (uint8_t screen = 0; screen < g_nb_screen; screen++) {
            if (!is_combined) {
                if (!g_split_two_screens_to_panels && is_panel1) {
                    continue;
                }
                if (g_split_two_screens_to_panels && (int)screen != panel) {
                    continue;
                }
            }

            uint16_t id = (uint16_t)(2 + screen * 4);
            float u = (float)bi[id + 0];
            float v = (float)bi[id + 1];
            float w = (float)bi[id + 2];
            float h = (float)bi[id + 3];

            float u0, v0, u1, v1;
            calc_uv_rect(texW, texH, u, v, w, h, u0, v0, u1, v1);

            uint16_t scale = g_segment_info[2] ? g_segment_info[2] : 1;
            float dw = (float)g_segment_info[4 + screen * 2] / (float)scale;
            float dh = (float)g_segment_info[5 + screen * 2] / (float)scale;

            float gx = 0.0f;
            float gy = 0.0f;
            get_screen_base_global(screen, gx, gy);
            float dx = to_local_x(gx);
            float dy = to_local_y(gy);

            append_quad_ndc_uv_canvas(bg_verts, contentW, contentH, dx, dy, dw, dh, u0, v0, u1, v1);
        }

        draw_vertices(r.tex_background, bg_verts);
    }

    // Segments layer.
    if (panel_is_game && g_segment_info[0] > 0 && g_segment_info[1] > 0) {
        const bool is_mask = (g_segment_info[3] & 0x01) != 0;

        static constexpr uint32_t kSegmentColorRgb[5] = {
            0x080908u,
            0x9992e7u,
            0x58b9a0u,
            0xff677cu,
            0x3db8e4u,
        };

        static thread_local std::vector<RenderVertex> seg_verts;
        seg_verts.clear();

        static thread_local std::vector<RenderVertex> seg_color_verts[5];
        bool has_colored_segments = false;
        if (!is_mask) {
            for (auto& v : seg_color_verts) {
                v.clear();
            }
        }

        std::shared_ptr<const std::vector<Segment>> meta;
        std::shared_ptr<std::vector<uint8_t>> on;
        {
            std::lock_guard<std::mutex> snap_lock(g_segment_snapshot_mutex);
            meta = g_segments_meta;
            on = g_seg_on_front;
        }

        const size_t seg_count = meta ? meta->size() : 0;
        seg_verts.reserve(seg_count * 6);
        if (!is_mask) {
            for (auto& v : seg_color_verts) {
                v.reserve(seg_count * 6);
            }
        }

        static thread_local std::vector<RenderVertex> seg_mark_verts;
        static thread_local std::vector<RenderVertex> seg_shadow_verts;
        if (!is_mask) {
            seg_mark_verts.clear();
            seg_shadow_verts.clear();
            seg_mark_verts.reserve(seg_count * 6);
            seg_shadow_verts.reserve(seg_count * 6);
        } else {
            seg_mark_verts.clear();
            seg_shadow_verts.clear();
        }

        uint16_t scale = g_segment_info[2] ? g_segment_info[2] : 1;
        float texW = (float)g_segment_info[0];
        float texH = (float)g_segment_info[1];

        if (meta) for (size_t si = 0; si < meta->size(); si++) {
            const auto& seg = (*meta)[si];
            const bool seg_on = (on && si < on->size()) ? ((*on)[si] != 0) : false;
            if (!is_combined) {
                if (!g_split_two_screens_to_panels && is_panel1) {
                    continue;
                }
                if (g_split_two_screens_to_panels && (int)seg.screen != panel) {
                    continue;
                }
            }

            float base_gx = 0.0f;
            float base_gy = 0.0f;
            get_screen_base_global(seg.screen, base_gx, base_gy);

            float sx2 = (float)seg.pos_scr[0] / (float)scale + base_gx;
            float sy2 = (float)seg.pos_scr[1] / (float)scale + base_gy;
            float sw = (float)seg.size_tex[0] / (float)scale;
            float sh = (float)seg.size_tex[1] / (float)scale;

            float sx_local = to_local_x(sx2);
            float sy_local = to_local_y(sy2);

            float u0 = 0.0f;
            float v0 = 0.0f;
            float u1 = 1.0f;
            float v1 = 1.0f;

            if (r.tex_segments != 0) {
                float u = (float)seg.pos_tex[0];
                float v = (float)seg.pos_tex[1];
                float w = (float)seg.size_tex[0];
                float h = (float)seg.size_tex[1];
                calc_uv_rect(texW, texH, u, v, w, h, u0, v0, u1, v1);
            }

            if (!is_mask) {
                append_quad_ndc_uv_canvas(seg_mark_verts, contentW, contentH, sx_local, sy_local, sw, sh, u0, v0, u1, v1);

                if (seg_on) {
                    append_quad_ndc_uv_canvas(seg_shadow_verts, contentW, contentH, sx_local + 2.0f, sy_local + 2.0f, sw, sh, u0, v0, u1, v1);
                }
            }

            if (seg_on) {
                if (!is_mask && seg.color_index > 0 && seg.color_index < 5) {
                    has_colored_segments = true;
                    append_quad_ndc_uv_canvas(seg_color_verts[seg.color_index], contentW, contentH, sx_local, sy_local, sw, sh, u0, v0, u1, v1);
                } else {
                    append_quad_ndc_uv_canvas(seg_verts, contentW, contentH, sx_local, sy_local, sw, sh, u0, v0, u1, v1);
                }
            }
        }

        float seg_r = br * 0.12f;
        float seg_g = bgc * 0.12f;
        float seg_b = bb * 0.12f;

        GLuint seg_tex = (r.tex_segments != 0) ? r.tex_segments : r.tex_white;

        if (!is_mask) {
            float mark_a = (float)g_settings.segment_marking_alpha / 255.0f;
            if (mark_a > 0.0f && !seg_mark_verts.empty()) {
                float m = 16.0f / 255.0f;
                glUniform4f(r.uMul, m, m, m, mark_a);
                draw_vertices(seg_tex, seg_mark_verts);
            }

            if (!seg_shadow_verts.empty()) {
                float s = 17.0f / 255.0f;
                glUniform4f(r.uMul, s, s, s, (float)0x18 / 255.0f);
                draw_vertices(seg_tex, seg_shadow_verts);
            }
        }

        if (is_mask) {
            if (r.uAlphaOnly >= 0) {
                glUniform1f(r.uAlphaOnly, 0.0f);
            }
            glUniform4f(r.uMul, 1.0f, 1.0f, 1.0f, 1.0f);
        } else {
            if (r.uAlphaOnly >= 0) {
                glUniform1f(r.uAlphaOnly, 0.0f);
            }
            glUniform4f(r.uMul, seg_r, seg_g, seg_b, 1.0f);
        }
        draw_vertices(seg_tex, seg_verts);

        if (!is_mask && has_colored_segments) {
            if (r.uAlphaOnly >= 0) {
                glUniform1f(r.uAlphaOnly, 1.0f);
            }
            for (uint32_t ci = 1; ci < 5; ci++) {
                if (seg_color_verts[ci].empty()) {
                    continue;
                }
                const uint32_t rgb = kSegmentColorRgb[ci];
                const float cr = (float)((rgb >> 16) & 0xFF) / 255.0f;
                const float cg = (float)((rgb >> 8) & 0xFF) / 255.0f;
                const float cb = (float)(rgb & 0xFF) / 255.0f;
                glUniform4f(r.uMul, cr, cg, cb, 1.0f);
                draw_vertices(seg_tex, seg_color_verts[ci]);
            }
            if (r.uAlphaOnly >= 0) {
                glUniform1f(r.uAlphaOnly, 0.0f);
            }
        }

        if (r.uAlphaOnly >= 0) {
            glUniform1f(r.uAlphaOnly, 0.0f);
        }
    }

    const bool want_console = (!g_split_two_screens_to_panels) && (is_combined || is_panel1);
    if (want_console && kRenderConsoleOverlay && r.tex_console != 0 && g_game && g_game->console_info && r.tex_console_w > 0 && r.tex_console_h > 0) {
        if (r.uAlphaOnly >= 0) {
            glUniform1f(r.uAlphaOnly, 0.0f);
        }
        glUniform4f(r.uMul, 1.0f, 1.0f, 1.0f, 1.0f);
        std::vector<RenderVertex> cs_verts;
        cs_verts.reserve(6);

        const uint16_t* ci = g_game->console_info;
        float texW = (float)r.tex_console_w;
        float texH = (float)r.tex_console_h;

        float u = (float)ci[2];
        float v = (float)ci[3];
        float w = (float)ci[4];
        float h = (float)ci[5];

        float u0, v0, u1, v1;
        calc_uv_rect(texW, texH, u, v, w, h, u0, v0, u1, v1);

        uint16_t scale = g_segment_info[2] ? g_segment_info[2] : 1;
        float dst_w = w / (float)scale;
        float dst_h = h / (float)scale;

        float dx = to_local_x(g_bottom_off_x);
        float dy = to_local_y(g_bottom_off_y);
        append_quad_ndc_uv_canvas(cs_verts, contentW, contentH, dx, dy, dst_w, dst_h, u0, v0, u1, v1);
        draw_vertices(r.tex_console, cs_verts);
    }

    glBindVertexArray(0);
}

} // namespace

void yokoi_frontend_init() {
    (void)get_or_create_gl_for_current_context();

    if (!g_core_inited) {
        load_settings();

        if (!gw_pack::is_loaded()) {
            std::string err;
            const std::string p = storage_path("yokoi_pack_rgds.ykp");
            (void)gw_pack::load(p, &err);
        }

        g_app_mode.store(MODE_MENU_SELECT);
        if (get_nb_name() > 0) {
            uint8_t idx = yokoi_get_default_game_index_for_android();
            g_menu_load_choice.store(yokoi_get_default_menu_load_choice_for_game(idx));
            yokoi_menu_select_game_by_index(idx);
        }
        g_core_inited = true;
    }

    ensure_emu_thread_started();
}

void yokoi_frontend_shutdown() {
    std::lock_guard<std::mutex> cpu_lock(g_cpu_mutex);
    stop_emu_thread();
}

void yokoi_frontend_resize(int width, int height) {
    GlResources* r = get_or_create_gl_for_current_context();
    if (!r) {
        return;
    }
    r->width = width;
    r->height = height;
    glViewport(0, 0, r->width, r->height);
}

void yokoi_frontend_render(int panel) {
    GlResources* r = get_or_create_gl_for_current_context();
    if (!r) {
        return;
    }
    render_frame(*r, panel);
}

void yokoi_frontend_set_textures(
    uint32_t segmentTex, int segmentW, int segmentH,
    uint32_t backgroundTex, int backgroundW, int backgroundH,
    uint32_t consoleTex, int consoleW, int consoleH) {
    GlResources* r = get_or_create_gl_for_current_context();
    if (!r) {
        return;
    }

    r->tex_segments = (GLuint)segmentTex;
    r->tex_segments_w = segmentW;
    r->tex_segments_h = segmentH;

    r->tex_background = (GLuint)backgroundTex;
    r->tex_background_w = backgroundW;
    r->tex_background_h = backgroundH;

    r->tex_console = (GLuint)consoleTex;
    r->tex_console_w = consoleW;
    r->tex_console_h = consoleH;

    if (r->tex_segments_w > 0 && r->tex_segments_h > 0) {
        g_segment_info[0] = (uint16_t)r->tex_segments_w;
        g_segment_info[1] = (uint16_t)r->tex_segments_h;
    }
}

void yokoi_frontend_set_ui_texture(uint32_t uiTex, int uiW, int uiH) {
    GlResources* r = get_or_create_gl_for_current_context();
    if (!r) {
        return;
    }
    r->tex_ui = (GLuint)uiTex;
    r->tex_ui_w = uiW;
    r->tex_ui_h = uiH;
}

uint32_t yokoi_frontend_get_texture_generation() {
    return g_texture_generation.load();
}

int yokoi_frontend_audio_get_sample_rate() {
    const int rate = yokoi_audio_get_source_rate();
    return (rate > 0 ? rate : 32768);
}

int yokoi_frontend_audio_read(int16_t* pcm, int frames) {
    if (!pcm || frames <= 0) return 0;
    yokoi_audio_read(pcm, frames);
    return frames;
}

void yokoi_frontend_set_emulation_driver_panel(int panel) {
    int p = panel;
    if (p < -1) p = -1;
    if (p > 1) p = 1;
    g_emulation_driver_panel.store(p);
}
