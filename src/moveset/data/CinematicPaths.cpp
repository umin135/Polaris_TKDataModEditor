// CinematicPaths.cpp -- builds the cinematic manifest for a moveset and writes it via
// SaveCineManifest (shared serializer). Path formats reconstructed 1:1 from the decompiled game
// builders; season folder resolved from live game memory (SeasonFolderResolver). See the header +
// _references/CinematicSequence_Paths_RE.md. Redirect/override model: _references/Cinematic_Redirect_Plan.md.
#include "moveset/data/CinematicPaths.h"
#include "moveset/data/MotbinData.h"
#include "moveset/data/CinematicManifest.h"
#include "moveset/data/MovesetDataDict.h"
#include <set>
#include <utility>
#include <string>
#include <cstdio>
#include <windows.h>

static constexpr uint32_t kProp838E = 0x838E; // "Trigger cinematic camera" (rage / throw)
static constexpr uint32_t kProp8313 = 0x8313; // "Set Drama Type"  (1 = intro, 2 = outro)
static constexpr uint32_t kProp8314 = 0x8314; // "Set Drama No."

// side ids (match sub_141818DD0 offsets): rage=0, outro=1, intro=2, throw=3.
enum { SIDE_RAGE = 0, SIDE_OUTRO = 1, SIDE_INTRO = 2, SIDE_THROW = 3 };

// One resolved sequence path + season-folder validity/existence.
struct Seq {
    std::string path;
    bool exists = false, checked = false;
    bool valid  = true; // false = resolver active but (side,index) out of the live game's arrays
};

// path tails below the season bucket (filenames match the game / export dump exactly)
static std::string RageTail (const std::string& c, const char* tok, const char* cam)
{ return c + "/rage/" + tok + "/" + c + "_rage_" + tok + "_" + cam + "_master"; }
static std::string ThrowTail(const std::string& c, const std::string& nn, const char* cam)
{ return c + "/throw/" + nn + "/" + c + "_throw_" + nn + "_" + cam + "_master"; }
static std::string DemoTail (const std::string& c, const char* type, const std::string& nn)
{ return c + "/" + type + "/" + nn + "/" + c + "_" + type + "_" + nn + "_master"; }

// Resolves the season folder (via resolver) + builds the package path; cross-checks existence vs dump.
static Seq MakeSeq(const SeasonFolderResolver& resolver, const std::string& exportRoot,
                   const char* base, int side, int index, const std::string& tail)
{
    Seq s;
    std::string folder;
    if (resolver) {
        folder = resolver(side, index);
        if (folder.empty()) { s.valid = false; folder = "polaris"; } // out of range in live data
    } else {
        folder = "polaris";
    }
    s.path = std::string("/Game/cinematics/") + base + "/" + folder + "/" + tail;
    if (!exportRoot.empty()) {
        s.checked = true;
        std::string disk = exportRoot + "\\" + base + "\\" + folder + "\\" + tail + ".json";
        for (char& c : disk) if (c == '/') c = '\\';
        DWORD attr = GetFileAttributesA(disk.c_str());
        s.exists = (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
    }
    return s;
}

// Map a 0x838E params[0] value to (category, sub). 5/6/7 = rage pre/finish/finishko, 8+ = throw NN.
static bool MapProp838E(uint32_t param, std::string& cat, std::string& sub, int& throwNN)
{
    switch (param) {
        case 5: cat = "rage"; sub = "pre";      return true;
        case 6: cat = "rage"; sub = "finish";   return true;
        case 7: cat = "rage"; sub = "finishko"; return true;
        default: break;
    }
    if (param >= 8) {
        throwNN = static_cast<int>(param) - 8;
        char b[8]; snprintf(b, sizeof(b), "%02d", throwNN);
        cat = "throw"; sub = b;
        return true;
    }
    return false;
}

void WriteCinematicSequencesJson(const MotbinData& data,
                                 const std::string& code,
                                 const std::string& folderPath,
                                 const SeasonFolderResolver& resolver,
                                 const std::string& exportRoot)
{
    if (code.empty()) return;

    // Re-extraction always overwrites: a fresh manifest is generated from live game data, discarding
    // any previous overrides / added slots (cinematic.json is otherwise saved only via the moveset save).

    // ---- scan the moveset (per-move-group pairing) for used props ----
    auto buildGroups = [](const std::vector<ParsedExtraProp>& blk, bool extra) {
        std::vector<std::pair<uint32_t, uint32_t>> g; uint32_t start = 0;
        for (uint32_t i = 0; i < (uint32_t)blk.size(); ++i) {
            bool term = extra ? (blk[i].type == 0 && blk[i].id == 0) : (blk[i].id == 1100);
            if (term) { g.push_back({ start, i - start + 1 }); start = i + 1; }
        }
        if (start < (uint32_t)blk.size()) g.push_back({ start, (uint32_t)blk.size() - start });
        return g;
    };
    const auto ep = buildGroups(data.extraPropBlock, true);
    const auto sp = buildGroups(data.startPropBlock, false);
    const auto np = buildGroups(data.endPropBlock, false);
    auto findGroup = [](const std::vector<std::pair<uint32_t, uint32_t>>& g, uint32_t idx) -> int {
        for (int i = 0; i < (int)g.size(); ++i) if (g[i].first == idx) return i; return -1; };

    std::set<uint32_t> camParams;                 // 0x838E values used (param 0 skipped)
    std::set<std::pair<int, uint32_t>> dramaPairs; // (type, no)
    auto scanGroup = [&](const std::vector<ParsedExtraProp>& blk,
                         const std::vector<std::pair<uint32_t, uint32_t>>& groups, uint32_t idx) {
        if (idx == 0xFFFFFFFF) return;
        int gi = findGroup(groups, idx); if (gi < 0) return;
        uint32_t s = groups[gi].first, n = groups[gi].second; int lastType = -1;
        for (uint32_t k = s; k < s + n && k < (uint32_t)blk.size(); ++k) {
            const auto& p = blk[k];
            if (p.id == kProp838E) { if (p.value != 0) camParams.insert(p.value); }
            else if (p.id == kProp8313) lastType = (int)p.value;
            else if (p.id == kProp8314) dramaPairs.insert({ lastType, p.value });
        }
    };
    for (const auto& m : data.moves) {
        scanGroup(data.extraPropBlock, ep, m.extra_prop_idx);
        scanGroup(data.startPropBlock, sp, m.start_prop_idx);
        scanGroup(data.endPropBlock,   np, m.end_prop_idx);
    }

    // ---- build the manifest ----
    CineManifest man;
    man.code = code;
    man.folderSource = resolver ? "game-runtime" : "assumed-polaris";

    std::vector<uint32_t>    excl838E;
    std::vector<std::string> exclDrama;
    std::set<int>            throwPresent, introPresent, outroPresent; // avoid dup with added lists

    auto pushRage = [&](const std::string& sub) {
        for (const char* cam : { "cam1p", "cam2p" }) {
            Seq s = MakeSeq(resolver, exportRoot, "game", SIDE_RAGE, 0, RageTail(code, sub.c_str(), cam));
            CineManifestEntry e;
            e.group = "rage"; e.sub = sub; e.cam = cam;
            e.id = "rage_" + sub + "_" + cam; e.src = s.path;
            e.hasExists = s.checked; e.exists = s.exists;
            man.entries.push_back(std::move(e));
        }
    };
    auto pushThrow = [&](int nn, bool added) {
        char nnb[8]; snprintf(nnb, sizeof(nnb), "%02d", nn);
        for (const char* cam : { "cam1p", "cam2p" }) {
            Seq s = MakeSeq(resolver, exportRoot, "game", SIDE_THROW, nn, ThrowTail(code, nnb, cam));
            CineManifestEntry e;
            e.group = "throw"; e.cam = cam; e.num = nn; e.added = added;
            e.id = std::string("throw_") + nnb + "_" + cam; e.src = s.path;
            e.hasExists = s.checked; e.exists = s.exists;
            man.entries.push_back(std::move(e));
        }
        throwPresent.insert(nn);
    };

    for (uint32_t pv : camParams) {           // std::set is sorted: rage (5-7) then throw (8+)
        std::string cat, sub; int nn = -1;
        if (!MapProp838E(pv, cat, sub, nn)) continue;
        if (cat == "rage") { pushRage(sub); continue; }
        Seq probe = MakeSeq(resolver, exportRoot, "game", SIDE_THROW, nn, ThrowTail(code, sub, "cam1p"));
        if (!probe.valid) { excl838E.push_back(pv); continue; } // out of range -> stage gimmick etc.
        pushThrow(nn, false);
    }

    for (const auto& pr : dramaPairs) {
        int type = pr.first; uint32_t no = pr.second;
        if (type != 1 && type != 2) {
            const char* tl = MovesetDataDict::Get().GetDramaTypeLabel((uint32_t)type);
            char lbl[64];
            if (tl && tl[0]) snprintf(lbl, sizeof(lbl), "%s (type %d, no %u)", tl, type, no);
            else             snprintf(lbl, sizeof(lbl), "type %d, no %u", type, no);
            exclDrama.push_back(lbl);
            continue;
        }
        const char* seg = (type == 1) ? "intro" : "outro";
        const char* tok = (type == 1) ? "sta"   : "win";
        int side        = (type == 1) ? SIDE_INTRO : SIDE_OUTRO;
        char nnb[8]; snprintf(nnb, sizeof(nnb), "%02u", no);
        Seq s = MakeSeq(resolver, exportRoot, "demo", side, (int)no, DemoTail(code, tok, nnb));
        if (!s.valid) { char l[24]; snprintf(l, sizeof(l), "%s_%u", seg, no); exclDrama.push_back(l); continue; }
        CineManifestEntry e;
        e.group = seg; e.num = (int)no;
        e.id = std::string(seg) + "_" + std::to_string(no); e.src = s.path;
        e.hasExists = s.checked; e.exists = s.exists;
        man.entries.push_back(std::move(e));
        (type == 1 ? introPresent : outroPresent).insert((int)no);
    }

    // excluded block (verbatim text) -- info only, never a redirect target
    std::string ex = "\"note\": \"Not per-character camera targets (stage-gimmick / FATE / story / out-of-range).\",\n    \"prop_838E\": [";
    for (size_t i = 0; i < excl838E.size(); ++i) ex += (i ? ", " : "") + std::to_string(excl838E[i]);
    ex += "],\n    \"drama\": [";
    for (size_t i = 0; i < exclDrama.size(); ++i) ex += (i ? ", " : "") + std::string("\"") + exclDrama[i] + "\"";
    ex += "]";
    man.excludedRaw = ex;

    SaveCineManifest(folderPath, man);
}
