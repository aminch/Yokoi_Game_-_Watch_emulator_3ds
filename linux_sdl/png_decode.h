#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Decodes a PNG image from memory into RGBA8.
// Returns true on success; on failure, error_out may be set.
bool png_decode_rgba(const uint8_t* data, size_t size, int& out_w, int& out_h, std::vector<uint8_t>& out_rgba, std::string* error_out);
