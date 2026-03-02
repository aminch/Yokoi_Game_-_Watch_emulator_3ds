#include "png_decode.h"

#include <cstring>

#include <png.h>

namespace {
struct MemReader {
    const uint8_t* data = nullptr;
    size_t size = 0;
    size_t off = 0;
};

static void png_read_from_mem(png_structp png_ptr, png_bytep outBytes, png_size_t byteCountToRead) {
    MemReader* rd = reinterpret_cast<MemReader*>(png_get_io_ptr(png_ptr));
    if (!rd || !outBytes || byteCountToRead == 0) {
        png_error(png_ptr, "invalid read state");
        return;
    }
    if (rd->off + (size_t)byteCountToRead > rd->size) {
        png_error(png_ptr, "read beyond end");
        return;
    }
    std::memcpy(outBytes, rd->data + rd->off, (size_t)byteCountToRead);
    rd->off += (size_t)byteCountToRead;
}
}

bool png_decode_rgba(const uint8_t* data, size_t size, int& out_w, int& out_h, std::vector<uint8_t>& out_rgba, std::string* error_out) {
    out_w = 0;
    out_h = 0;
    out_rgba.clear();

    if (!data || size < 8) {
        if (error_out) *error_out = "input too small";
        return false;
    }
    if (png_sig_cmp((png_bytep)data, 0, 8) != 0) {
        if (error_out) *error_out = "not a png";
        return false;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        if (error_out) *error_out = "png_create_read_struct failed";
        return false;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        if (error_out) *error_out = "png_create_info_struct failed";
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        if (error_out && error_out->empty()) *error_out = "libpng error";
        return false;
    }

    MemReader rd;
    rd.data = data;
    rd.size = size;
    rd.off = 0;
    png_set_read_fn(png_ptr, &rd, png_read_from_mem);

    png_read_info(png_ptr, info_ptr);

    png_uint_32 w = 0;
    png_uint_32 h = 0;
    int bit_depth = 0;
    int color_type = 0;
    png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type, nullptr, nullptr, nullptr);

    // Normalize to 8-bit RGBA.
    if (bit_depth == 16) {
        png_set_strip_16(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }

    png_read_update_info(png_ptr, info_ptr);

    const png_size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    if (rowbytes == 0 || w == 0 || h == 0) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        if (error_out) *error_out = "invalid dimensions";
        return false;
    }

    out_rgba.resize((size_t)rowbytes * (size_t)h);
    std::vector<png_bytep> rows((size_t)h);
    for (size_t y = 0; y < (size_t)h; y++) {
        rows[y] = (png_bytep)(out_rgba.data() + y * (size_t)rowbytes);
    }

    png_read_image(png_ptr, rows.data());
    png_read_end(png_ptr, nullptr);
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

    out_w = (int)w;
    out_h = (int)h;

    // Ensure RGBA8 row stride is exactly w*4 (some PNGs may pad).
    if ((size_t)rowbytes != (size_t)out_w * 4) {
        std::vector<uint8_t> tight((size_t)out_w * 4 * (size_t)out_h);
        for (int y = 0; y < out_h; y++) {
            std::memcpy(tight.data() + (size_t)y * (size_t)out_w * 4, out_rgba.data() + (size_t)y * (size_t)rowbytes, (size_t)out_w * 4);
        }
        out_rgba.swap(tight);
    }

    return true;
}
