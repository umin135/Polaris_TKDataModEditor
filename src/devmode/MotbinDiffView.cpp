#ifdef _DEBUG

#include "MotbinDiffView.h"
#include "Config.h"
#include "imgui/imgui.h"

#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cstdint>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <map>

// -----------------------------------------------------------------------------
//  Constants
// -----------------------------------------------------------------------------

static constexpr size_t kMotbinBase = 0x318;

// -----------------------------------------------------------------------------
//  Low-level helpers
// -----------------------------------------------------------------------------

static std::vector<uint8_t> LoadFile(const char* path)
{
    FILE* f = nullptr;
    fopen_s(&f, path, "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return {}; }
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    fread(buf.data(), 1, sz, f);
    fclose(f);
    return buf;
}

template<typename T>
static T RAt(const uint8_t* b, size_t sz, size_t off)
{
    T v = {};
    if (off + sizeof(T) <= sz) memcpy(&v, b + off, sizeof(T));
    return v;
}

// Header stores (block_file_offset - kMotbinBase); recover actual file offset.
static uint64_t BlockOff(const uint8_t* b, size_t sz, size_t hdrOff)
{
    return RAt<uint64_t>(b, sz, hdrOff) + kMotbinBase;
}
static uint64_t BlockCnt(const uint8_t* b, size_t sz, size_t cntOff)
{
    return RAt<uint64_t>(b, sz, cntOff);
}

// -----------------------------------------------------------------------------
//  Move struct field name table (4-byte granularity)
//  Offsets that fall in the hitbox/unk5 bulk ranges are handled separately.
// -----------------------------------------------------------------------------

struct MoveField { uint32_t off; const char* name; };
static const MoveField kMoveFields[] = {
    // Encrypted name_key block (8 XOR slots, 0x00-0x1F)
    { 0x00, "name_key_block[0]" },   { 0x04, "name_key_block[1]" },
    { 0x08, "name_key_block[2]" },   { 0x0C, "name_key_block[3]" },
    { 0x10, "name_key_block[4]" },   { 0x14, "name_key_block[5]" },
    { 0x18, "name_key_block[6]" },   { 0x1C, "name_key_block[7]" },
    // Encrypted anim_key block (0x20-0x3F)
    { 0x20, "anim_key_block[0]" },   { 0x24, "anim_key_block[1]" },
    { 0x28, "anim_key_block[2]" },   { 0x2C, "anim_key_block[3]" },
    { 0x30, "anim_key_block[4]" },   { 0x34, "anim_key_block[5]" },
    { 0x38, "anim_key_block[6]" },   { 0x3C, "anim_key_block[7]" },
    // Abs pointers (zeroed in state-1)
    { 0x40, "name_idx_abs.lo" },     { 0x44, "name_idx_abs.hi" },
    { 0x48, "anim_idx_abs.lo" },     { 0x4C, "anim_idx_abs.hi" },
    // Anim enc fields
    { 0x50, "anim_addr_enc1" },      { 0x54, "anim_addr_enc2" },
    // Encrypted vuln block (0x58-0x77)
    { 0x58, "vuln_block[0]" },       { 0x5C, "vuln_block[1]" },
    { 0x60, "vuln_block[2]" },       { 0x64, "vuln_block[3]" },
    { 0x68, "vuln_block[4]" },       { 0x6C, "vuln_block[5]" },
    { 0x70, "vuln_block[6]" },       { 0x74, "vuln_block[7]" },
    // Encrypted hitlevel block (0x78-0x97)
    { 0x78, "hitlevel_block[0]" },   { 0x7C, "hitlevel_block[1]" },
    { 0x80, "hitlevel_block[2]" },   { 0x84, "hitlevel_block[3]" },
    { 0x88, "hitlevel_block[4]" },   { 0x8C, "hitlevel_block[5]" },
    { 0x90, "hitlevel_block[6]" },   { 0x94, "hitlevel_block[7]" },
    // Cancel pointers
    { 0x98, "cancel_addr.lo" },      { 0x9C, "cancel_addr.hi" },
    { 0xA0, "cancel2_addr.lo" },     { 0xA4, "cancel2_addr.hi" },
    { 0xA8, "u1.lo" },               { 0xAC, "u1.hi" },
    { 0xB0, "u2.lo" },               { 0xB4, "u2.hi" },
    { 0xB8, "u3.lo" },               { 0xBC, "u3.hi" },
    { 0xC0, "u4.lo" },               { 0xC4, "u4.hi" },
    { 0xC8, "u6" },                  { 0xCC, "transition|_0xCE" },
    // Encrypted char_id block (0xD0-0xEF)
    { 0xD0, "char_id_block[0]" },    { 0xD4, "char_id_block[1]" },
    { 0xD8, "char_id_block[2]" },    { 0xDC, "char_id_block[3]" },
    { 0xE0, "char_id_block[4]" },    { 0xE4, "char_id_block[5]" },
    { 0xE8, "char_id_block[6]" },    { 0xEC, "char_id_block[7]" },
    // Encrypted ordinal_id block (0xF0-0x10F)
    { 0xF0, "ordinal_id_block[0]" }, { 0xF4, "ordinal_id_block[1]" },
    { 0xF8, "ordinal_id_block[2]" }, { 0xFC, "ordinal_id_block[3]" },
    { 0x100, "ordinal_id_block[4]" },{ 0x104, "ordinal_id_block[5]" },
    { 0x108, "ordinal_id_block[6]" },{ 0x10C, "ordinal_id_block[7]" },
    // Pointers and timing
    { 0x110, "hit_cond_addr.lo" },   { 0x114, "hit_cond_addr.hi" },
    { 0x118, "_0x118" },             { 0x11C, "_0x11C" },
    { 0x120, "anim_len" },
    { 0x124, "airborne_start" },     { 0x128, "airborne_end" },
    { 0x12C, "ground_fall" },
    { 0x130, "voicelip_addr.lo" },   { 0x134, "voicelip_addr.hi" },
    { 0x138, "extra_prop_addr.lo" }, { 0x13C, "extra_prop_addr.hi" },
    { 0x140, "start_prop_addr.lo" }, { 0x144, "start_prop_addr.hi" },
    { 0x148, "end_prop_addr.lo" },   { 0x14C, "end_prop_addr.hi" },
    { 0x150, "u15" },                { 0x154, "_0x154" },
    { 0x158, "startup" },            { 0x15C, "recovery" },
    // Collision / distance (after hitbox/unk5 bulk ranges)
    { 0x2E0, "collision|distance" },
    { 0x444, "u18" },
};
static constexpr int kMoveFieldCount = (int)(sizeof(kMoveFields) / sizeof(kMoveFields[0]));

static const char* FindMoveFieldName(uint32_t off)
{
    for (int i = 0; i < kMoveFieldCount; ++i)
        if (kMoveFields[i].off == off) return kMoveFields[i].name;
    return nullptr;
}

// -----------------------------------------------------------------------------
//  Block definitions
// -----------------------------------------------------------------------------

struct BlockDef { const char* name; size_t ptrOff; size_t cntOff; size_t stride; };
static const BlockDef kBlockDefs[] = {
    { "reaction_list",  0x168, 0x178, 0x70  },
    { "requirements",   0x180, 0x188, 0x14  },
    { "hit_conditions", 0x190, 0x198, 0x18  },
    { "projectiles",    0x1A0, 0x1A8, 0xE0  },
    { "pushbacks",      0x1B0, 0x1B8, 0x10  },
    { "pushback_extra", 0x1C0, 0x1C8, 0x02  },
    { "cancels",        0x1D0, 0x1D8, 0x28  },
    { "group_cancels",  0x1E0, 0x1E8, 0x28  },
    { "cancel_extra",   0x1F0, 0x1F8, 0x04  },
    { "extra_props",    0x200, 0x208, 0x28  },
    { "start_props",    0x210, 0x218, 0x20  },
    { "end_props",      0x220, 0x228, 0x20  },
    { "moves",          0x230, 0x238, 0x448 },
    { "voicelips",      0x240, 0x248, 0x0C  },
    { "input_seqs",     0x250, 0x258, 0x10  },
    { "input_extra",    0x260, 0x268, 0x08  },
    { "parry",          0x270, 0x278, 0x04  },
    { "throw_extra",    0x280, 0x288, 0x0C  },
    { "throws",         0x290, 0x298, 0x10  },
    { "dialogues",      0x2A0, 0x2A8, 0x18  },
};

// -----------------------------------------------------------------------------
//  Core diff logic
// -----------------------------------------------------------------------------

static std::string BuildDiffReport(const char* pathA, const char* pathB,
                                   const std::string& outDir)
{
    auto fileA = LoadFile(pathA);
    auto fileB = LoadFile(pathB);

    if (fileA.empty()) return "Error: Could not load original file.";
    if (fileB.empty()) return "Error: Could not load extracted file.";
    if (fileA.size() < kMotbinBase) return "Error: Original file too small.";
    if (fileB.size() < kMotbinBase) return "Error: Extracted file too small.";

    const uint8_t* A  = fileA.data();
    const uint8_t* B  = fileB.data();
    const size_t   szA = fileA.size();
    const size_t   szB = fileB.size();

    std::ostringstream rpt;

    // Header
    {
        time_t now = time(nullptr);
        char tbuf[64] = {};
        ctime_s(tbuf, sizeof(tbuf), &now);
        rpt << "=== MOTBIN DIFF REPORT ===\n";
        rpt << "Generated: " << tbuf;
        rpt << "Original:  " << pathA << "\n";
        rpt << "Extracted: " << pathB << "\n";
        rpt << "Size: A=" << szA << "  B=" << szB << "\n\n";
    }

    // Header scalar fields
    rpt << "=== HEADER FIELDS ===\n";
    auto CmpHdr32 = [&](size_t off, const char* name) {
        uint32_t vA = RAt<uint32_t>(A, szA, off);
        uint32_t vB = RAt<uint32_t>(B, szB, off);
        char buf[128];
        snprintf(buf, sizeof(buf), "  [%s] [0x%03zX] %-16s  A=0x%08X  B=0x%08X\n",
                 vA == vB ? "OK" : "!!", off, name, vA, vB);
        rpt << buf;
    };
    CmpHdr32(0x04, "_0x4");
    CmpHdr32(0x08, "signature");
    rpt << "\n";

    // Block counts
    rpt << "=== BLOCK COUNTS ===\n";
    int cntMismatches = 0;
    for (const BlockDef& bd : kBlockDefs)
    {
        uint64_t cA = BlockCnt(A, szA, bd.cntOff);
        uint64_t cB = BlockCnt(B, szB, bd.cntOff);
        bool ok = (cA == cB);
        if (!ok) cntMismatches++;
        char buf[128];
        snprintf(buf, sizeof(buf), "  [%s] %-16s  A=%-6llu  B=%-6llu\n",
                 ok ? "OK" : "!!",
                 bd.name,
                 (unsigned long long)cA,
                 (unsigned long long)cB);
        rpt << buf;
    }
    if (cntMismatches == 0)
        rpt << "  All block counts match.\n";
    rpt << "\n";

    // -- Move-level diff ------------------------------------------------------
    uint64_t moveCntA  = BlockCnt(A, szA, 0x238);
    uint64_t moveCntB  = BlockCnt(B, szB, 0x238);
    uint64_t moveCnt   = std::min(moveCntA, moveCntB);
    uint64_t moveOffA  = BlockOff(A, szA, 0x230);
    uint64_t moveOffB  = BlockOff(B, szB, 0x230);

    rpt << "=== MOVE DIFFERENCES (" << moveCnt << " moves compared";
    if (moveCntA != moveCntB)
        rpt << ", A=" << moveCntA << " B=" << moveCntB << " MISMATCH";
    rpt << ") ===\n";

    // frequency map: field-offset -> count of moves where it differs
    std::map<uint32_t, int> freqMap;

    int totalMovesDiff = 0;
    int totalDiffDwords = 0;

    struct DiffEntry { uint32_t off; uint32_t vA; uint32_t vB; std::string name; };

    for (uint64_t mi = 0; mi < moveCnt; ++mi)
    {
        size_t baseA = static_cast<size_t>(moveOffA + mi * 0x448);
        size_t baseB = static_cast<size_t>(moveOffB + mi * 0x448);
        if (baseA + 0x448 > szA || baseB + 0x448 > szB) break;

        const uint8_t* mA = A + baseA;
        const uint8_t* mB = B + baseB;

        std::vector<DiffEntry> diffs;
        int hitboxDiffs = 0, unk5Diffs = 0;

        for (uint32_t off = 0; off < 0x448; off += 4)
        {
            uint32_t vA, vB;
            memcpy(&vA, mA + off, 4);
            memcpy(&vB, mB + off, 4);
            if (vA == vB) continue;

            // Bulk range: hitbox slots (0x160-0x2DF) and unk5 (0x2E4-0x443)
            if (off >= 0x160 && off < 0x2E0) {
                hitboxDiffs++;
                freqMap[0x160]++;  // bucket all hitbox diffs under 0x160
                continue;
            }
            if (off >= 0x2E4 && off < 0x444) {
                unk5Diffs++;
                freqMap[0x2E4]++;  // bucket all unk5 diffs under 0x2E4
                continue;
            }

            const char* fn = FindMoveFieldName(off);
            std::string name;
            if (fn) {
                name = fn;
            } else {
                char tmp[32];
                snprintf(tmp, sizeof(tmp), "unknown[0x%03X]", off);
                name = tmp;
            }
            diffs.push_back({ off, vA, vB, name });
            freqMap[off]++;
        }

        int moveDiffCount = (int)diffs.size() + (hitboxDiffs > 0 ? 1 : 0) + (unk5Diffs > 0 ? 1 : 0);
        if (moveDiffCount == 0) continue;

        totalMovesDiff++;
        totalDiffDwords += (int)diffs.size() + hitboxDiffs + unk5Diffs;

        char hdr[80];
        snprintf(hdr, sizeof(hdr), "\nMove #%04llu:\n", (unsigned long long)mi);
        rpt << hdr;

        for (const DiffEntry& d : diffs)
        {
            char line[128];
            snprintf(line, sizeof(line),
                     "  [0x%03X] %-26s  A=0x%08X  B=0x%08X\n",
                     d.off, d.name.c_str(), d.vA, d.vB);
            rpt << line;
        }
        if (hitboxDiffs > 0)
        {
            char line[80];
            snprintf(line, sizeof(line),
                     "  [0x160-0x2DF] hitbox_slots:  %d/96 dwords differ\n", hitboxDiffs);
            rpt << line;
        }
        if (unk5Diffs > 0)
        {
            char line[80];
            snprintf(line, sizeof(line),
                     "  [0x2E4-0x443] unk5:          %d/88 dwords differ\n", unk5Diffs);
            rpt << line;
        }
    }

    rpt << "\nSummary: " << totalMovesDiff << "/" << moveCnt
        << " moves differ.  Total differing dwords: " << totalDiffDwords << "\n";

    // Field frequency table
    if (!freqMap.empty())
    {
        rpt << "\n=== FIELD FREQUENCY (sorted by occurrence) ===\n";

        // Build sorted list: descending by count
        std::vector<std::pair<int, uint32_t>> sorted;
        for (std::map<uint32_t,int>::const_iterator it = freqMap.begin(); it != freqMap.end(); ++it)
            sorted.push_back(std::make_pair(it->second, it->first));
        std::sort(sorted.begin(), sorted.end(),
                  [](const std::pair<int,uint32_t>& a, const std::pair<int,uint32_t>& b){
                      return a.first > b.first;
                  });

        for (size_t si = 0; si < sorted.size(); ++si)
        {
            int      cnt = sorted[si].first;
            uint32_t off = sorted[si].second;

            const char* fn = nullptr;
            if (off == 0x160) fn = "hitbox_slots (range)";
            else if (off == 0x2E4) fn = "unk5 (range)";
            else fn = FindMoveFieldName(off);

            char name[40] = {};
            if (fn) snprintf(name, sizeof(name), "%s", fn);
            else    snprintf(name, sizeof(name), "unknown[0x%03X]", off);

            char line[128];
            snprintf(line, sizeof(line),
                     "  [0x%03X] %-28s  %d/%llu moves\n",
                     off, name, cnt, (unsigned long long)moveCnt);
            rpt << line;
        }
    }

    // -- Non-move block diffs (element-level byte comparison) ----------------
    rpt << "\n=== OTHER BLOCK ELEMENT DIFFS ===\n";
    bool anyNonMoveDiff = false;
    for (const BlockDef& bd : kBlockDefs)
    {
        if (bd.cntOff == 0x238) continue;  // moves already handled

        uint64_t cA  = BlockCnt(A, szA, bd.cntOff);
        uint64_t cB  = BlockCnt(B, szB, bd.cntOff);
        uint64_t cnt = std::min(cA, cB);
        if (cnt == 0) continue;

        uint64_t oA = BlockOff(A, szA, bd.ptrOff);
        uint64_t oB = BlockOff(B, szB, bd.ptrOff);
        int diffElems = 0;

        for (uint64_t ei = 0; ei < cnt; ++ei)
        {
            size_t eA = static_cast<size_t>(oA + ei * bd.stride);
            size_t eB = static_cast<size_t>(oB + ei * bd.stride);
            if (eA + bd.stride > szA || eB + bd.stride > szB) break;
            if (memcmp(A + eA, B + eB, bd.stride) != 0) diffElems++;
        }

        if (diffElems > 0)
        {
            anyNonMoveDiff = true;
            char line[128];
            snprintf(line, sizeof(line),
                     "  [!!] %-16s  %d/%llu elements differ\n",
                     bd.name, diffElems, (unsigned long long)cnt);
            rpt << line;
        }
    }
    if (!anyNonMoveDiff)
        rpt << "  All non-move block elements are identical.\n";

    rpt << "\n=== END OF REPORT ===\n";

    // Write report file
    std::string outPath = outDir;
    if (!outPath.empty() && outPath.back() != '\\' && outPath.back() != '/')
        outPath += '\\';
    outPath += "report.txt";

    FILE* f = nullptr;
    fopen_s(&f, outPath.c_str(), "wb");
    if (!f) return "Error: Could not write to " + outPath;

    std::string text = rpt.str();
    fwrite(text.data(), 1, text.size(), f);
    fclose(f);
    return "Report written to: " + outPath;
}

// -----------------------------------------------------------------------------
//  File picker
// -----------------------------------------------------------------------------

static std::string BrowseForMotbin()
{
    IFileDialog* pfd = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&pfd))))
        return {};

    COMDLG_FILTERSPEC filters[] = {
        { L"Motbin files (*.motbin)", L"*.motbin" },
        { L"All files (*.*)",         L"*.*"       },
    };
    pfd->SetFileTypes(2, filters);
    pfd->SetTitle(L"Select motbin file");

    std::string result;
    if (SUCCEEDED(pfd->Show(nullptr)))
    {
        IShellItem* psi = nullptr;
        if (SUCCEEDED(pfd->GetResult(&psi)))
        {
            PWSTR pStr = nullptr;
            if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pStr)) && pStr)
            {
                int n = WideCharToMultiByte(CP_UTF8, 0, pStr, -1, nullptr, 0, nullptr, nullptr);
                if (n > 0) {
                    result.resize(static_cast<size_t>(n - 1));
                    WideCharToMultiByte(CP_UTF8, 0, pStr, -1, &result[0], n, nullptr, nullptr);
                }
                CoTaskMemFree(pStr);
            }
            psi->Release();
        }
    }
    pfd->Release();
    return result;
}

// -----------------------------------------------------------------------------
//  UI
// -----------------------------------------------------------------------------

void MotbinDiffView::Render()
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
    ImGui::TextUnformatted("[ DEBUG ] Motbin Diff");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled(
        "Select two state-1 .motbin files to compare.\n"
        "The report is written to <movesetRootDir>\\report.txt.");
    ImGui::Spacing();

    const float btnW  = 80.0f;
    const float pathW = ImGui::GetContentRegionAvail().x
                      - btnW - ImGui::GetStyle().ItemSpacing.x;

    // -- Original motbin ------------------------------------------------------
    ImGui::TextUnformatted("Original motbin (working reference):");
    ImGui::SetNextItemWidth(pathW);
    ImGui::InputText("##pathA", m_pathA, sizeof(m_pathA));
    ImGui::SameLine();
    if (ImGui::Button("Browse##A", ImVec2(btnW, 0.0f)))
    {
        std::string p = BrowseForMotbin();
        if (!p.empty())
            strncpy_s(m_pathA, sizeof(m_pathA), p.c_str(), _TRUNCATE);
    }

    ImGui::Spacing();

    // -- Extracted motbin -----------------------------------------------------
    ImGui::TextUnformatted("Extracted motbin (generated by editor):");
    ImGui::SetNextItemWidth(pathW);
    ImGui::InputText("##pathB", m_pathB, sizeof(m_pathB));
    ImGui::SameLine();
    if (ImGui::Button("Browse##B", ImVec2(btnW, 0.0f)))
    {
        std::string p = BrowseForMotbin();
        if (!p.empty())
            strncpy_s(m_pathB, sizeof(m_pathB), p.c_str(), _TRUNCATE);
    }

    ImGui::Spacing();

    const std::string& rootDir = Config::Get().data.movesetRootDir;
    if (rootDir.empty())
        ImGui::TextDisabled("Output: (set Moveset Root in Settings first)");
    else
        ImGui::TextDisabled("Output: %s\\report.txt", rootDir.c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // -- Generate button ------------------------------------------------------
    const bool canGenerate = (m_pathA[0] != '\0') && (m_pathB[0] != '\0') && !rootDir.empty();
    if (!canGenerate) ImGui::BeginDisabled();
    if (ImGui::Button("Generate Report", ImVec2(160.0f, 0.0f)))
        RunReport();
    if (!canGenerate) ImGui::EndDisabled();

    if (!canGenerate && rootDir.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("<- set Moveset Root in Settings");
    }

    if (!m_status.empty())
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text,
            m_statusOk ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f)
                       : ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::TextUnformatted(m_status.c_str());
        ImGui::PopStyleColor();
    }
}

void MotbinDiffView::RunReport()
{
    m_status   = BuildDiffReport(m_pathA, m_pathB, Config::Get().data.movesetRootDir);
    m_statusOk = (m_status.find("Error") == std::string::npos);
}

#endif // _DEBUG
