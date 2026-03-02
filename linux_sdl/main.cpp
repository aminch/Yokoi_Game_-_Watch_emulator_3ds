#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include "std/gw_pack.h"
#include "std/platform_paths.h"
#include "std/settings.h"

#include "yokoi_controller_state.h"
#include "yokoi_input_mapping.h"

#include "frontend/yokoi_frontend_api.h"

#include "texture_pack_loader.h"

namespace {

static void log_line(const char* tag, const std::string& s) {
    std::fprintf(stderr, "%s: %s\n", tag ? tag : "yokoi", s.c_str());
}

static std::string default_pack_path() {
    return std::string("yokoi_pack_rgds.ykp");
}

static std::string default_storage_root() {
    return std::string("saves");
}

static uint32_t build_controller_mask(SDL_GameController* pad) {
    uint32_t mask = 0;
    if (!pad) return 0;

    auto b = [&](SDL_GameControllerButton btn) -> bool {
        return SDL_GameControllerGetButton(pad, btn) != 0;
    };

    if (b(SDL_CONTROLLER_BUTTON_DPAD_UP)) mask |= CTL_DPAD_UP;
    if (b(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) mask |= CTL_DPAD_DOWN;
    if (b(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) mask |= CTL_DPAD_LEFT;
    if (b(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) mask |= CTL_DPAD_RIGHT;

    if (b(SDL_CONTROLLER_BUTTON_A)) mask |= CTL_A;
    if (b(SDL_CONTROLLER_BUTTON_B)) mask |= CTL_B;
    if (b(SDL_CONTROLLER_BUTTON_X)) mask |= CTL_X;
    if (b(SDL_CONTROLLER_BUTTON_Y)) mask |= CTL_Y;

    if (b(SDL_CONTROLLER_BUTTON_START)) mask |= CTL_START;
    if (b(SDL_CONTROLLER_BUTTON_BACK)) mask |= CTL_SELECT;

    if (b(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) mask |= CTL_L1;

    return mask;
}

struct AudioState {
    int src_rate = 32768;
    int out_rate = 0;
    float src_pos = 0.0f;
    int16_t prev = 0;
    bool has_prev = false;
};

static void sdl_audio_cb(void* userdata, Uint8* stream, int len) {
    AudioState* st = reinterpret_cast<AudioState*>(userdata);
    if (!st || !stream || len <= 0) return;
    int frames = len / (int)sizeof(int16_t);
    int16_t* out = reinterpret_cast<int16_t*>(stream);

    if (frames <= 0) return;

    const int out_rate = st->out_rate > 0 ? st->out_rate : st->src_rate;
    const int src_rate = st->src_rate > 0 ? st->src_rate : 32768;

    if (out_rate == src_rate) {
        int got = yokoi_frontend_audio_read(out, frames);
        for (int i = got; i < frames; i++) out[i] = 0;
        return;
    }

    // Linear resample from source to output.
    const float step = (float)src_rate / (float)out_rate;

    int16_t s0 = st->has_prev ? st->prev : 0;
    int16_t s1 = 0;
    bool have_s1 = false;

    for (int i = 0; i < frames; i++) {
        // Ensure we have s1.
        if (!have_s1) {
            int got = yokoi_frontend_audio_read(&s1, 1);
            if (got <= 0) s1 = s0;
            have_s1 = true;
        }

        float frac = st->src_pos;
        out[i] = (int16_t)((float)s0 + ((float)s1 - (float)s0) * frac);

        st->src_pos += step;
        while (st->src_pos >= 1.0f) {
            st->src_pos -= 1.0f;
            s0 = s1;
            st->prev = s0;
            st->has_prev = true;
            have_s1 = false;
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    std::string pack_path = default_pack_path();
    if (argc >= 2) {
        pack_path = argv[1];
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        log_line("SDL", std::string("SDL_Init failed: ") + SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

    SDL_Window* win = SDL_CreateWindow(
        "Yokoi",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280,
        720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!win) {
        log_line("SDL", std::string("CreateWindow failed: ") + SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext glctx = SDL_GL_CreateContext(win);
    if (!glctx) {
        log_line("SDL", std::string("CreateContext failed: ") + SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(win, glctx);
    SDL_GL_SetSwapInterval(1);

    // Verify EGL context availability (renderer tracks by EGLContext).
    if (eglGetCurrentContext() == EGL_NO_CONTEXT) {
        log_line("EGL", "eglGetCurrentContext returned EGL_NO_CONTEXT (SDL must use EGL for GLES)");
    }

    set_storage_root(default_storage_root());
    load_settings();

    {
        std::string err;
        if (!gw_pack::is_loaded()) {
            if (!gw_pack::load(pack_path, &err)) {
                log_line("ROMPACK", std::string("Failed to load pack: ") + err);
                SDL_GL_DeleteContext(glctx);
                SDL_DestroyWindow(win);
                SDL_Quit();
                return 2;
            }
        }
    }

    // Core init (starts emulation thread).
    yokoi_frontend_init();

    // Audio
    AudioState audio;
    audio.src_rate = yokoi_frontend_audio_get_sample_rate();

    SDL_AudioSpec want{};
    want.freq = audio.src_rate;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = sdl_audio_cb;
    want.userdata = &audio;

    SDL_AudioSpec have{};
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (dev != 0) {
        audio.out_rate = have.freq;
        SDL_PauseAudioDevice(dev, 0);
    } else {
        log_line("SDL", std::string("Audio disabled: ") + SDL_GetError());
    }

    SDL_GameController* pad = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            pad = SDL_GameControllerOpen(i);
            if (pad) break;
        }
    }

    YokoiTextureSet tex;
    uint32_t last_tex_gen = 0;

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
            if (e.type == SDL_CONTROLLERDEVICEADDED && !pad) {
                if (SDL_IsGameController(e.cdevice.which)) {
                    pad = SDL_GameControllerOpen(e.cdevice.which);
                }
            }
            if (e.type == SDL_CONTROLLERDEVICEREMOVED && pad) {
                SDL_JoystickID jid = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad));
                if (jid == e.cdevice.which) {
                    SDL_GameControllerClose(pad);
                    pad = nullptr;
                }
            }
        }

        // Input
        yokoi_controller_set_mask(build_controller_mask(pad));

        // Resize
        int w = 0, h = 0;
        SDL_GL_GetDrawableSize(win, &w, &h);
        yokoi_frontend_resize(w, h);

        // Texture reload
        uint32_t gen = yokoi_frontend_get_texture_generation();
        if (gen != last_tex_gen) {
            last_tex_gen = gen;
            std::string err;
            if (yokoi_load_textures_from_pack(tex, &err)) {
                yokoi_frontend_set_textures(
                    tex.seg, tex.seg_w, tex.seg_h,
                    tex.bg, tex.bg_w, tex.bg_h,
                    tex.cs, tex.cs_w, tex.cs_h);
            } else {
                if (!err.empty()) {
                    log_line("TEX", err);
                }
            }
        }

        // Render
        yokoi_frontend_render(-1);
        SDL_GL_SwapWindow(win);
    }

    yokoi_delete_textures(tex);
    yokoi_frontend_shutdown();

    if (dev != 0) {
        SDL_CloseAudioDevice(dev);
    }
    if (pad) {
        SDL_GameControllerClose(pad);
    }

    SDL_GL_DeleteContext(glctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
