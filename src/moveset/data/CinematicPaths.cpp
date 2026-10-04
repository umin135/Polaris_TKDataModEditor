// CinematicPaths.cpp -- builds the cinematic manifest for a moveset and writes it via
// SaveCineManifest (shared serializer). Path formats reconstructed 1:1 from the decompiled game
// builders; season folder / existence from CinematicSeqDB, gated by the live-game season table
// (ResolveCineSeq; resolution order in the header). See the header +
// _references/CinematicSequence_Paths_RE.md. Redirect/override model: _references/Cinematic_Redirect_Plan.md.
#include "moveset/data/CinematicPaths.h"
#include "moveset/data/MotbinData.h"
#include "moveset/data/CinematicManifest.h"
#include "moveset/data/MovesetDataDict.h"
#include "moveset/data/CinematicSeqDB.h"
#include <set>
#include <utility>
#include <string>
#include <cstdio>
#include <windows.h>

static constexpr uint32_t kProp838E = 0x838E; // "Trigger cinematic camera" (rage / throw)
static constexpr uint32_t kProp8313 = 0x8313; // "Set Drama Type"  (1 = intro, 2 = outro)
static constexpr uint32_t kProp8314 = 0x8314; // "Set Drama No."

std::string CineRageTail(const std::string& c, const std::string& sub, const char* cam)
{ return c + "/rage/" + sub + "/" + c + "_rage_" + sub + "_" + cam + "_master"; }

std::string CineThrowTail(const std::string& c, int nn, const char* cam)
{
    char b[8]; snprintf(b, sizeof(b), "%02d", nn);
    return c + "/throw/" + b + "/" + c + "_throw_" + b + "_" + cam + "_master";
}

std::string CineDemoTail(const std::string& c, const char* tok, int no)
{
    char b[8]; snprintf(b, sizeof(b), "%02d", no);
    return c + "/" + tok + "/" + b + "/" + c + "_" + tok + "_" + b + "_master";
}

CineSeqResolve ResolveCineSeq(const CineSeasonTable& table, const char* base,
                              int side, int index, const std::string& tail)
{
    CineSeqResolve r;
    // 1. live table identifies empty slots (out of range / value < 0) regardless of the DB
    int live = 0;
    if (table.valid) {
        const auto& arr = table.side[side];
        if (index < 0 || index >= (int)arr.size() || arr[index] < 0) {
            r.state = CineSeqState::EmptyInGame;
            return r;
        }
        live = arr[index];
    }
    // 2. sequence DB: authoritative season folder + existence
    const CinematicSeqDB& db = CinematicSeqDB::Get();
    if (db.IsLoaded()) {
        r.folder = db.FindSeason(base, tail);
        r.state  = r.folder.empty() ? CineSeqState::NoAsset : CineSeqState::Ok;
        r.verified = !r.folder.empty();
        return r;
    }
    // 3. fallback: season from the live table
    if (table.valid) {
        char b[16];
        if (live > 0) snprintf(b, sizeof(b), "polaris%02d", live);
        else          snprintf(b, sizeof(b), "polaris");
        r.folder = b;
        r.state  = CineSeqState::Ok;
        return r;
    }
    // 4. nothing to resolve with -- don't guess
    r.state = CineSeqState::Unresolved;
    return r;
}

static const char* StateReason(CineSeqState s)
{
    switch (s) {
        case CineSeqState::EmptyInGame: return "empty in game data";
        case CineSeqState::NoAsset:     return "no such sequence asset";
        case CineSeqState::Unresolved:  return "unresolved (no sequence DB / game data)";
        default:                        return "";
    }
}

std::string WriteCinematicSequencesJson(const MotbinData& data,
                                        const std::string& code,
                                        const std::string& folderPath,
                                        const CineSeasonTable& seasons,
                                        const std::string& exportRoot)
{
    if (code.empty()) return {};

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
    const bool dbOk = CinematicSeqDB::Get().IsLoaded();
    CineManifest man;
    man.code    = code;
    man.seasons = seasons;
    man.folderSource = dbOk ? (seasons.valid ? "sequence-db+game-runtime" : "sequence-db")
                            : (seasons.valid ? "game-runtime" : "unresolved");

    std::vector<uint32_t>    excl838E;    // 0x838E values with no sequence at all
    std::vector<std::string> exclDrama;   // non intro/outro drama types + dropped intro/outro
    std::vector<std::string> exclSeq;     // "id: reason" for every dropped sequence
    int unresolved = 0;

    // Resolves one sequence and appends it as an entry; returns false (and records why) if dropped.
    auto addSeq = [&](CineManifestEntry e, const char* base, int side, int index, const std::string& tail) {
        CineSeqResolve r = ResolveCineSeq(seasons, base, side, index, tail);
        if (r.state != CineSeqState::Ok) {
            if (r.state == CineSeqState::Unresolved) ++unresolved;
            exclSeq.push_back(e.id + ": " + StateReason(r.state));
            return false;
        }
        e.src = std::string("/Game/cinematics/") + base + "/" + r.folder + "/" + tail;
        if (r.verified) {
            e.hasExists = true; e.exists = true;
        } else if (!exportRoot.empty()) {   // fallback path: optional dump cross-check
            std::string disk = exportRoot + "\\" + base + "\\" + r.folder + "\\" + tail + ".json";
            for (char& c : disk) if (c == '/') c = '\\';
            DWORD attr = GetFileAttributesA(disk.c_str());
            e.hasExists = true;
            e.exists = (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
        }
        man.entries.push_back(std::move(e));
        return true;
    };

    // 0x838E: 5/6/7 = rage pre/finish/finishko (side 0, index 0), 8+ = throw NN (side 3, index NN)
    for (uint32_t pv : camParams) {           // std::set is sorted: rage (5-7) then throw (8+)
        int any = 0;
        if (pv >= 5 && pv <= 7) {
            const char* sub = pv == 5 ? "pre" : pv == 6 ? "finish" : "finishko";
            for (const char* cam : { "cam1p", "cam2p" }) {
                CineManifestEntry e;
                e.group = "rage"; e.sub = sub; e.cam = cam;
                e.id = std::string("rage_") + sub + "_" + cam;
                any += addSeq(e, "game", CINE_SIDE_RAGE, 0, CineRageTail(code, sub, cam));
            }
        } else if (pv >= 8) {
            int nn = (int)pv - 8;
            char nnb[8]; snprintf(nnb, sizeof(nnb), "%02d", nn);
            for (const char* cam : { "cam1p", "cam2p" }) {
                CineManifestEntry e;
                e.group = "throw"; e.cam = cam; e.num = nn;
                e.id = std::string("throw_") + nnb + "_" + cam;
                any += addSeq(e, "game", CINE_SIDE_THROW, nn, CineThrowTail(code, nn, cam));
            }
        } else {
            continue; // 1-4: not a per-character camera slot
        }
        if (!any) excl838E.push_back(pv);   // stage gimmick / empty slot / missing asset
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
        int side        = (type == 1) ? CINE_SIDE_INTRO : CINE_SIDE_OUTRO;
        CineManifestEntry e;
        e.group = seg; e.num = (int)no;
        e.id = std::string(seg) + "_" + std::to_string(no);
        if (!addSeq(e, "demo", side, (int)no, CineDemoTail(code, tok, (int)no)))
            exclDrama.push_back(std::string(seg) + "_" + std::to_string(no));
    }

    // excluded block (verbatim text) -- info only, never a redirect target
    auto quoteList = [](const std::vector<std::string>& v) {
        std::string r;
        for (size_t i = 0; i < v.size(); ++i) r += (i ? ", " : "") + std::string("\"") + v[i] + "\"";
        return r;
    };
    std::string ex = "\"note\": \"Not per-character camera targets (stage-gimmick / FATE / story / empty slot / missing asset).\",\n    \"prop_838E\": [";
    for (size_t i = 0; i < excl838E.size(); ++i) ex += (i ? ", " : "") + std::to_string(excl838E[i]);
    ex += "],\n    \"drama\": [" + quoteList(exclDrama) + "]";
    ex += ",\n    \"sequences\": [" + quoteList(exclSeq) + "]";
    man.excludedRaw = ex;

    SaveCineManifest(folderPath, man);

    char msg[200];
    if (unresolved > 0)
        snprintf(msg, sizeof(msg), "\n[!] Cinematics UNRESOLVED (no sequence DB, no game data): %d sequence(s) not written. "
                 "Restore res/cinematics/data.json or re-extract in Practice mode.", unresolved);
    else
        snprintf(msg, sizeof(msg), " | cinematics: %d sequence(s) [%s]",
                 (int)man.entries.size(), man.folderSource.c_str());
    return msg;
}
