#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Convert TK7/TTT2 anim bytes (FBF magic C8 00 or VLC-KEF magic 64 00) to TK8 26-bone PANM.
// Faithful port of t7_data/umin_convertor/converter.py convert_file(..., output_26bone=True).
bool ConvertT7AnimToPanm(const uint8_t* src, size_t srcLen,
                         std::vector<uint8_t>& outPanm,
                         std::string& errorMsg);

// Optional: estimate byte size of a T7 anim blob starting at src (for process dumps).
// Returns 0 if unknown. FBF: exact from header. KEF: 0 (caller must bound another way).
size_t EstimateT7AnimSize(const uint8_t* src, size_t srcLen);
