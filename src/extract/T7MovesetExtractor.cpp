// T7MovesetExtractor.cpp — Tekken 7 runtime moveset extraction
#include "T7MovesetExtractor.h"
#include "T7Constants.h"
#include "moveset/serialize/T7ToT8Convert.h"
#include "moveset/data/MotbinData.h"
#include "moveset/data/T7AliasDict.h"
#include "moveset/data/AnmbinRebuild.h"
#include "moveset/data/AnimNameDB.h"
#include "moveset/anim/T7AnimToPanm.h"
#include "FbsDataDict.h"
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <cctype>
#include <unordered_map>

using namespace T7;

// -------------------------------------------------------------
//  Helpers
// -------------------------------------------------------------

template<typename T>
static bool ReadArr(const T7MemorySource& mem, uintptr_t addr,
                    std::vector<T>& out, uint64_t count, std::string& err)
{
    out.clear();
    if (count == 0) return true;
    if (count > kMaxBlockCount) {
        err = "Block count too large: " + std::to_string(count);
        return false;
    }
    size_t bytes = static_cast<size_t>(count * sizeof(T));
    if (bytes > kMaxReadBytes) {
        err = "Block byte size exceeds cap.";
        return false;
    }
    out.resize(static_cast<size_t>(count));
    if (!mem.Read(addr, out.data(), bytes)) {
        err = "Failed to read block at 0x" +
              [&]{ char b[32]; snprintf(b,sizeof(b),"%llX",(unsigned long long)addr); return std::string(b); }();
        out.clear();
        return false;
    }
    return true;
}

static uint32_t PtrToIdx(uintptr_t ptr, uintptr_t blockBase,
                         uint64_t count, size_t stride)
{
    if (ptr == 0 || blockBase == 0 || count == 0 || stride == 0)
        return 0xFFFFFFFF;
    if (ptr < blockBase) return 0xFFFFFFFF;
    uint64_t off = ptr - blockBase;
    if (off % stride != 0) return 0xFFFFFFFF;
    uint64_t idx = off / stride;
    if (idx >= count) return 0xFFFFFFFF;
    return static_cast<uint32_t>(idx);
}

static std::string ReadCString(const T7MemorySource& mem, uintptr_t addr)
{
    if (addr == 0) return {};
    char buf[kMaxStringLen + 1] = {};
    if (!mem.Read(addr, buf, kMaxStringLen))
        return {};
    buf[kMaxStringLen] = '\0';
    // Require printable ASCII start
    size_t len = 0;
    while (len < kMaxStringLen && buf[len]) {
        unsigned char c = static_cast<unsigned char>(buf[len]);
        if (c < 0x20 || c > 0x7E) { buf[len] = '\0'; break; }
        ++len;
    }
    return std::string(buf, len);
}

static std::string StripBrackets(std::string s)
{
    if (s.size() >= 2 && s.front() == '[' && s.back() == ']')
        s = s.substr(1, s.size() - 2);
    return s;
}

static std::string SanitizeFolderName(std::string s)
{
    for (char& c : s) {
        if (c == '<' || c == '>' || c == ':' || c == '"' ||
            c == '/' || c == '\\' || c == '|' || c == '?' || c == '*')
            c = '_';
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '.'))
        s.pop_back();
    if (s.empty()) s = "Unknown";
    return s;
}

// -------------------------------------------------------------

bool T7MovesetExtractor::Connect()
{
    if (m_proc.valid) CloseGameProcess(m_proc);
    if (!FindGameProcessByName(kProcessName, m_proc)) {
        m_statusMsg = "Tekken 7 not running (TekkenGame-Win64-Shipping.exe not found).";
        return false;
    }
    char buf[128];
    snprintf(buf, sizeof(buf),
             "Tekken 7 detected. Module base: 0x%llX",
             (unsigned long long)m_proc.moduleBase);
    m_statusMsg = buf;
    return true;
}

void T7MovesetExtractor::Disconnect()
{
    CloseGameProcess(m_proc);
    m_slots[0] = {};
    m_slots[1] = {};
    m_statusMsg = "Disconnected.";
}

bool T7MovesetExtractor::ReadSlot(int slotIndex, T7PlayerSlotInfo& slot)
{
    slot = {};
    slot.slotIndex = slotIndex;
    if (!m_proc.valid || slotIndex < 0 || slotIndex > 1) return false;

    uintptr_t player1 = m_proc.moduleBase + kP1Rva;
    uintptr_t player  = player1 + static_cast<uintptr_t>(slotIndex) * kPlayerSize;
    slot.playerAddr = player;

    uint32_t fighterId = 0xFFFF;
    if (!ReadGameValue(m_proc, player + kFighterIdOffset, fighterId))
        return false;
    slot.charaId = fighterId;

    uintptr_t moveset = 0;
    if (!ReadGamePointer(m_proc, player + kMovesetOffset, moveset) || moveset == 0)
        return false;
    slot.movesetAddr = moveset;

    // Validate HID magic
    char magic[4] = {};
    if (!ReadGameMemory(m_proc, moveset + kMagicOffset, magic, 4))
        return false;
    if (memcmp(magic, kMagic, 4) != 0)
        return false;

    uint8_t initFlag = 0;
    ReadGameValue(m_proc, moveset + kIsInitializedOff, initFlag);
    // Soft check — some builds may still have valid data

    // Move count from table entry 12
    uint64_t moveCount = 0;
    ReadGameValue(m_proc, moveset + kTableOff + kTblMove * kTableEntrySize + 8, moveCount);
    if (moveCount == 0 || moveCount > kMaxBlockCount)
        return false;
    slot.moveCount = static_cast<uint32_t>(moveCount);

    // Character name at +0x2E8
    T7ProcessMemorySource mem(m_proc);
    std::string name = ReadCString(mem, moveset + kCharNameOff);
    name = StripBrackets(std::move(name));
    if (name.empty()) {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "chara_%u", fighterId);
        name = tmp;
    }
    slot.charaName = name;
    slot.valid = true;
    return true;
}

void T7MovesetExtractor::RefreshSlots()
{
    if (!m_proc.valid) return;
    ReadSlot(0, m_slots[0]);
    ReadSlot(1, m_slots[1]);
}

bool T7MovesetExtractor::ExtractMoveset(const T7MemorySource& mem,
                                        uintptr_t movesetAddr, uint32_t fighterId,
                                        Moveset& out, std::string& errorMsg)
{
    out = {};
    out.baseAddr = movesetAddr;
    out.fighterIdFromPlayer = fighterId;

    // Header
    std::vector<uint8_t> hdr(kMovesetInfoSize);
    if (!mem.Read(movesetAddr, hdr.data(), kMovesetInfoSize)) {
        errorMsg = "Failed to read MovesetInfo header.";
        return false;
    }
    if (memcmp(hdr.data() + kMagicOffset, kMagic, 4) != 0) {
        errorMsg = "Invalid T7 moveset magic (expected HID).";
        return false;
    }

    memcpy(out.origAliases,    hdr.data() + kOrigAliasesOff,    sizeof(out.origAliases));
    memcpy(out.currentAliases, hdr.data() + kCurrentAliasesOff, sizeof(out.currentAliases));
    memcpy(out.unknownAliases, hdr.data() + kUnknownAliasesOff, sizeof(out.unknownAliases));
    memcpy(&out.encodedCharId, hdr.data() + kEncodedCharIdOff, 4);

    out.characterName = StripBrackets(ReadCString(mem, movesetAddr + kCharNameOff));

    // Header string pointers at +0x08/+0x10/+0x18/+0x20 (absolute)
    auto rdPtr = [&](size_t off) -> uintptr_t {
        uint64_t v = 0;
        memcpy(&v, hdr.data() + off, 8);
        return static_cast<uintptr_t>(v);
    };
    // character_name_addr is at 0x08 — we already prefer +0x2E8
    out.characterCreator = ReadCString(mem, rdPtr(0x10));
    out.date             = ReadCString(mem, rdPtr(0x18));
    out.fullDate         = ReadCString(mem, rdPtr(0x20));

    // Table entries
    struct Entry { uintptr_t addr; uint64_t count; };
    Entry entries[kTableEntryCount] = {};
    for (int i = 0; i < kTableEntryCount; ++i) {
        size_t off = kTableOff + i * kTableEntrySize;
        uint64_t a = 0, c = 0;
        memcpy(&a, hdr.data() + off, 8);
        memcpy(&c, hdr.data() + off + 8, 8);
        entries[i] = { static_cast<uintptr_t>(a), c };
        if (c > kMaxBlockCount) {
            errorMsg = "Corrupt table count at entry " + std::to_string(i);
            return false;
        }
    }

    auto load = [&](int idx, auto& vec) -> bool {
        using T = typename std::decay_t<decltype(vec)>::value_type;
        return ReadArr<T>(mem, entries[idx].addr, vec, entries[idx].count, errorMsg);
    };

    if (!load(kTblReactions, out.reactions)) return false;
    if (!load(kTblRequirement, out.requirements)) return false;
    if (!load(kTblHitCondition, out.hitConditions)) return false;
    if (!load(kTblProjectile, out.projectiles)) return false;
    if (!load(kTblPushback, out.pushbacks)) return false;
    if (!load(kTblPushbackExtra, out.pushbackExtras)) return false;
    if (!load(kTblCancel, out.cancels)) return false;
    if (!load(kTblGroupCancel, out.groupCancels)) return false;
    if (!load(kTblCancelExtra, out.cancelExtras)) return false;
    if (!load(kTblExtraMoveProperty, out.extraMoveProperties)) return false;
    if (!load(kTblMoveBeginningProp, out.moveBeginningProps)) return false;
    if (!load(kTblMoveEndingProp, out.moveEndingProps)) return false;
    if (!load(kTblMove, out.moves)) return false;
    if (!load(kTblVoiceclip, out.voiceclips)) return false;
    if (!load(kTblInputSequence, out.inputSequences)) return false;
    if (!load(kTblInput, out.inputs)) return false;
    if (!load(kTblParryRelated, out.parryRelated)) return false;
    if (!load(kTblCameraData, out.cameraData)) return false;
    if (!load(kTblThrowCamera, out.throwCameras)) return false;

    // Resolve indexes
    auto resolveCancels = [&](const std::vector<Cancel>& src,
                              std::vector<uint32_t>& reqIdx,
                              std::vector<uint32_t>& exIdx) {
        reqIdx.resize(src.size());
        exIdx.resize(src.size());
        for (size_t i = 0; i < src.size(); ++i) {
            reqIdx[i] = PtrToIdx((uintptr_t)src[i].requirements_addr,
                                 entries[kTblRequirement].addr,
                                 entries[kTblRequirement].count, kStride[kTblRequirement]);
            exIdx[i]  = PtrToIdx((uintptr_t)src[i].extradata_addr,
                                 entries[kTblCancelExtra].addr,
                                 entries[kTblCancelExtra].count, kStride[kTblCancelExtra]);
        }
    };
    resolveCancels(out.cancels, out.cancelReqIdx, out.cancelExtraIdx);
    resolveCancels(out.groupCancels, out.groupCancelReqIdx, out.groupCancelExtraIdx);

    out.hitCondReqIdx.resize(out.hitConditions.size());
    out.hitCondReactIdx.resize(out.hitConditions.size());
    for (size_t i = 0; i < out.hitConditions.size(); ++i) {
        out.hitCondReqIdx[i] = PtrToIdx((uintptr_t)out.hitConditions[i].requirements_addr,
            entries[kTblRequirement].addr, entries[kTblRequirement].count, kStride[kTblRequirement]);
        out.hitCondReactIdx[i] = PtrToIdx((uintptr_t)out.hitConditions[i].reactions_addr,
            entries[kTblReactions].addr, entries[kTblReactions].count, kStride[kTblReactions]);
    }

    out.pushbackExtraIdx.resize(out.pushbacks.size());
    for (size_t i = 0; i < out.pushbacks.size(); ++i)
        out.pushbackExtraIdx[i] = PtrToIdx((uintptr_t)out.pushbacks[i].extradata_addr,
            entries[kTblPushbackExtra].addr, entries[kTblPushbackExtra].count, kStride[kTblPushbackExtra]);

    out.reactPushbackIdx.resize(out.reactions.size() * 7);
    for (size_t i = 0; i < out.reactions.size(); ++i)
        for (int p = 0; p < 7; ++p)
            out.reactPushbackIdx[i * 7 + p] = PtrToIdx((uintptr_t)out.reactions[i].pushbacks[p],
                entries[kTblPushback].addr, entries[kTblPushback].count, kStride[kTblPushback]);

    out.inputSeqInputIdx.resize(out.inputSequences.size());
    for (size_t i = 0; i < out.inputSequences.size(); ++i)
        out.inputSeqInputIdx[i] = PtrToIdx((uintptr_t)out.inputSequences[i].input_addr,
            entries[kTblInput].addr, entries[kTblInput].count, kStride[kTblInput]);

    out.throwCamDataIdx.resize(out.throwCameras.size());
    for (size_t i = 0; i < out.throwCameras.size(); ++i)
        out.throwCamDataIdx[i] = PtrToIdx((uintptr_t)out.throwCameras[i].cameradata_addr,
            entries[kTblCameraData].addr, entries[kTblCameraData].count, kStride[kTblCameraData]);

    auto resolveOther = [&](const std::vector<OtherMoveProperty>& src,
                            std::vector<uint32_t>& reqIdx) {
        reqIdx.resize(src.size());
        for (size_t i = 0; i < src.size(); ++i)
            reqIdx[i] = PtrToIdx((uintptr_t)src[i].requirements_addr,
                entries[kTblRequirement].addr, entries[kTblRequirement].count, kStride[kTblRequirement]);
    };
    resolveOther(out.moveBeginningProps, out.beginPropReqIdx);
    resolveOther(out.moveEndingProps, out.endPropReqIdx);

    // Moves: names + indexes
    out.moveNames.resize(out.moves.size());
    out.moveAnimNames.resize(out.moves.size());
    out.moveCancelIdx.resize(out.moves.size(), 0xFFFFFFFF);
    out.moveHitCondIdx.resize(out.moves.size(), 0xFFFFFFFF);
    out.moveVoiceIdx.resize(out.moves.size(), 0xFFFFFFFF);
    out.moveExtraPropIdx.resize(out.moves.size(), 0xFFFFFFFF);
    out.moveStartPropIdx.resize(out.moves.size(), 0xFFFFFFFF);
    out.moveEndPropIdx.resize(out.moves.size(), 0xFFFFFFFF);

    for (size_t i = 0; i < out.moves.size(); ++i) {
        const Move& m = out.moves[i];
        out.moveNames[i]     = ReadCString(mem, (uintptr_t)m.name_addr);
        out.moveAnimNames[i] = ReadCString(mem, (uintptr_t)m.anim_name_addr);
        out.moveCancelIdx[i] = PtrToIdx((uintptr_t)m.cancel_addr,
            entries[kTblCancel].addr, entries[kTblCancel].count, kStride[kTblCancel]);
        out.moveHitCondIdx[i] = PtrToIdx((uintptr_t)m.hit_condition_addr,
            entries[kTblHitCondition].addr, entries[kTblHitCondition].count, kStride[kTblHitCondition]);
        out.moveVoiceIdx[i] = PtrToIdx((uintptr_t)m.voicelip_addr,
            entries[kTblVoiceclip].addr, entries[kTblVoiceclip].count, kStride[kTblVoiceclip]);
        out.moveExtraPropIdx[i] = PtrToIdx((uintptr_t)m.extra_move_property_addr,
            entries[kTblExtraMoveProperty].addr, entries[kTblExtraMoveProperty].count, kStride[kTblExtraMoveProperty]);
        out.moveStartPropIdx[i] = PtrToIdx((uintptr_t)m.move_start_extraprop_addr,
            entries[kTblMoveBeginningProp].addr, entries[kTblMoveBeginningProp].count, kStride[kTblMoveBeginningProp]);
        out.moveEndPropIdx[i] = PtrToIdx((uintptr_t)m.move_end_extraprop_addr,
            entries[kTblMoveEndingProp].addr, entries[kTblMoveEndingProp].count, kStride[kTblMoveEndingProp]);
    }

    out.valid = true;
    return true;
}

bool T7MovesetExtractor::ExtractToFile(int slotIndex,
                                       const std::string& destFolder,
                                       std::string& errorMsg)
{
    if (!m_proc.valid) {
        errorMsg = "Not connected to Tekken 7.";
        return false;
    }
    T7PlayerSlotInfo& slot = m_slots[slotIndex];
    if (!slot.valid) {
        errorMsg = "Slot " + std::to_string(slotIndex + 1) + " has no valid T7 moveset.";
        return false;
    }

    T7ProcessMemorySource mem(m_proc);
    Moveset t7;
    if (!ExtractMoveset(mem, slot.movesetAddr, slot.charaId, t7, errorMsg))
        return false;

    // ---- Dump + convert body animations from Move.anim_addr (no MOTA) ----
    std::vector<uint32_t> animCrcByMove(t7.moves.size(), 0);
    std::vector<AnmbinPanmEntry> uniquePanms;
    std::unordered_map<uint32_t, size_t> crcToPoolIdx;
    int animOk = 0, animFail = 0, animSkip = 0;

    {
        // Collect unique non-null anim addresses and sort for size bounds.
        std::vector<uintptr_t> addrs;
        addrs.reserve(t7.moves.size());
        for (const auto& m : t7.moves) {
            if (m.anim_addr)
                addrs.push_back(static_cast<uintptr_t>(m.anim_addr));
        }
        std::sort(addrs.begin(), addrs.end());
        addrs.erase(std::unique(addrs.begin(), addrs.end()), addrs.end());

        constexpr size_t kMaxAnimBytes = 2u * 1024u * 1024u;
        std::unordered_map<uintptr_t, std::vector<uint8_t>> addrToBytes;

        for (size_t ai = 0; ai < addrs.size(); ++ai) {
            uintptr_t addr = addrs[ai];
            // Peek header for FBF size / magic
            uint8_t hdr[16] = {};
            if (!ReadGameMemory(m_proc, addr, hdr, sizeof(hdr)))
                continue;

            size_t want = EstimateT7AnimSize(hdr, sizeof(hdr));
            if (want == 0) {
                // KEF or unknown: bound by next unique address when contiguous.
                if (ai + 1 < addrs.size() && addrs[ai + 1] > addr) {
                    uint64_t gap = static_cast<uint64_t>(addrs[ai + 1] - addr);
                    if (gap > 0 && gap <= kMaxAnimBytes)
                        want = static_cast<size_t>(gap);
                }
                if (want == 0)
                    want = kMaxAnimBytes;
            }
            if (want < 16) continue;
            if (want > kMaxAnimBytes) want = kMaxAnimBytes;

            std::vector<uint8_t> blob(want);
            if (!ReadGameMemory(m_proc, addr, blob.data(), want)) {
                // Shrink on partial failure
                size_t trySz = want / 2;
                while (trySz >= 64) {
                    blob.resize(trySz);
                    if (ReadGameMemory(m_proc, addr, blob.data(), trySz))
                        break;
                    trySz /= 2;
                }
                if (trySz < 64) continue;
            }

            // Reject tiny/all-zero stubs.
            bool any = false;
            for (uint8_t b : blob) { if (b) { any = true; break; } }
            if (!any || blob.size() <= 8) continue;

            addrToBytes[addr] = std::move(blob);
        }

        // Convert each unique blob once
        std::unordered_map<uintptr_t, uint32_t> addrToCrc;
        for (auto& kv : addrToBytes) {
            std::vector<uint8_t> panm;
            std::string cerr;
            if (!ConvertT7AnimToPanm(kv.second.data(), kv.second.size(), panm, cerr)) {
                ++animFail;
                continue;
            }
            uint32_t crc = AnmbinCRC32(panm.data(), panm.size());
            addrToCrc[kv.first] = crc;
            if (crcToPoolIdx.find(crc) == crcToPoolIdx.end()) {
                crcToPoolIdx[crc] = uniquePanms.size();
                AnmbinPanmEntry e;
                e.crc32 = crc;
                e.panm = std::move(panm);
                uniquePanms.push_back(std::move(e));
            }
            ++animOk;
        }

        for (size_t i = 0; i < t7.moves.size(); ++i) {
            uintptr_t a = static_cast<uintptr_t>(t7.moves[i].anim_addr);
            if (!a) { ++animSkip; continue; }
            auto it = addrToCrc.find(a);
            if (it != addrToCrc.end())
                animCrcByMove[i] = it->second;
            else
                ++animSkip;
        }
    }

    return WriteConvertedFolder(t7, slot.charaId, slot.charaName, destFolder, errorMsg,
                                &animCrcByMove, &uniquePanms, animOk, animFail, animSkip);
}

bool T7MovesetExtractor::ConvertDumpToFile(const std::string& dumpBinPath,
                                           const std::string& destFolder,
                                           std::string& errorMsg)
{
    T7DumpFile dump;
    if (!dump.Load(dumpBinPath, errorMsg))
        return false;

    Moveset t7;
    if (!ExtractMoveset(dump, dump.movesetBase, dump.fighterId, t7, errorMsg))
        return false;

    std::string name = dump.characterName;
    if (name.empty())
        name = t7.characterName;
    if (name.empty())
        name = "chara_" + std::to_string(dump.fighterId);

    // No anim blobs in T7DUMP01 — convert motbin with name-hash anim keys.
    return WriteConvertedFolder(t7, dump.fighterId, name, destFolder, errorMsg,
                                nullptr, nullptr, 0, 0, 0);
}

bool T7MovesetExtractor::WriteConvertedFolder(
    const Moveset& t7,
    uint32_t fighterId,
    const std::string& charaName,
    const std::string& destFolder,
    std::string& errorMsg,
    const std::vector<uint32_t>* animCrcByMove,
    const std::vector<AnmbinPanmEntry>* uniquePanms,
    int animOk, int animFail, int animSkip)
{
    if (!T7AliasDict::Get().IsLoaded())
        T7AliasDict::Get().EnsureLoaded();

    MotbinData data;
    std::string convWarn;
    if (!ConvertT7ToMotbin(t7, fighterId, data, errorMsg, &convWarn, animCrcByMove))
        return false;

    std::string folder = destFolder;
    if (!folder.empty() && folder.back() != '\\' && folder.back() != '/')
        folder += '\\';
    std::string safeName = SanitizeFolderName(charaName);
    folder += "TK7_";
    folder += safeName;
    CreateDirectoryA(folder.c_str(), nullptr);

    data.folderPath = folder;
    if (!SaveMotbin(data)) {
        errorMsg = "SaveMotbin failed for " + folder;
        return false;
    }

    if (uniquePanms && !uniquePanms->empty() && animCrcByMove) {
        std::string aerr;
        if (!CreateAnmbinFromPanms(folder, *uniquePanms, *animCrcByMove, aerr)) {
            errorMsg = "CreateAnmbinFromPanms failed: " + aerr;
            return false;
        }
        AnimNameDB adb;
        std::unordered_map<uint32_t, std::string> crcNames;
        auto stripDvd = [](std::string s) {
            const char* suf = "(DVD)";
            size_t n = strlen(suf);
            while (s.size() >= n && s.compare(s.size() - n, n, suf) == 0)
                s.resize(s.size() - n);
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
                s.pop_back();
            return s;
        };
        for (size_t i = 0; i < t7.moves.size(); ++i) {
            if ((*animCrcByMove)[i] == 0) continue;
            uint32_t crc = (*animCrcByMove)[i];
            if (crcNames.count(crc)) continue;
            std::string nm = (i < t7.moveAnimNames.size())
                ? stripDvd(t7.moveAnimNames[i]) : "";
            if (nm.empty())
                nm = "anim_" + std::to_string(i);
            crcNames[crc] = std::move(nm);
        }
        for (const auto& kv : crcNames)
            adb.AddEntry(folder, kv.second, kv.first);
    }

    MotbinData check = LoadMotbin(folder);
    if (!check.loaded) {
        errorMsg = "Generated motbin failed to reload: " + check.errorMsg;
        return false;
    }
    if (check.moveCount != data.moveCount) {
        errorMsg = "Reload move count mismatch.";
        return false;
    }

    {
        uint32_t t8Id = T7AliasDict::Get().MapCharacterId(fighterId);
        const char* code = nullptr;
        if (t8Id != kPlaceholderCharId)
            code = FbsDataDict::Get().CharaCode(t8Id);
        std::string orig = code ? code : safeName;

        std::string iniPath = folder + "\\moveset.ini";
        FILE* f = nullptr;
        fopen_s(&f, iniPath.c_str(), "w");
        if (f) {
            fprintf(f, "[Info]\n");
            fprintf(f, "OriginalCharacter=%s\n", orig.c_str());
            fprintf(f, "Version=%s\n", kMovesetIniVersion);
            fprintf(f, "DefaultTarget=%s\n", orig.c_str());
            fprintf(f, "SourceGame=TK7\n");
            fprintf(f, "SourceFighterId=%u\n", fighterId);
            fprintf(f, "PlaceholderCharacterId=%u\n", kPlaceholderCharId);
            fclose(f);
        }
    }

    {
        std::string tkedit = folder + "\\.tkedit";
        CreateDirectoryA(tkedit.c_str(), nullptr);
        std::string jsonPath = tkedit + "\\Original_Moves.json";
        FILE* jf = nullptr;
        if (fopen_s(&jf, jsonPath.c_str(), "w") == 0 && jf) {
            fprintf(jf, "{\"name_keys\":[");
            for (size_t i = 0; i < data.moves.size(); ++i) {
                if (i) fprintf(jf, ",");
                fprintf(jf, "%u", data.moves[i].name_key);
            }
            fprintf(jf, "]}");
            fclose(jf);
        }
    }

    const size_t panmPool = uniquePanms ? uniquePanms->size() : 0;
    char status[640];
    snprintf(status, sizeof(status),
             "Converted -> TK7_%s  (moves=%u cancels=%zu reqs=%zu reactions=%zu | anims ok=%d fail=%d skip=%d pool=%zu)%s",
             safeName.c_str(),
             data.moveCount,
             data.cancelBlock.size(),
             data.requirementBlock.size(),
             data.reactionListBlock.size(),
             animOk, animFail, animSkip, panmPool,
             convWarn.empty() ? "" : (" | " + convWarn).c_str());
    m_statusMsg = status;
    return true;
}
