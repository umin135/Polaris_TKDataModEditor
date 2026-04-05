// AnimationManagerWindow.cpp
#include "AnimationManagerWindow.h"
#include "imgui/imgui.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <unordered_set>
#define NOMINMAX
#include <windows.h>

// -------------------------------------------------------------
//  Constructor
// -------------------------------------------------------------

AnimationManagerWindow::AnimationManagerWindow(const std::string& folderPath,
                                               const std::string& movesetName,
                                               int uid)
    : m_folderPath(folderPath)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "Animation Manager  [%s]##animmgr_%d", movesetName.c_str(), uid);
    m_windowId = buf;
}

// -------------------------------------------------------------
//  TryLoad
// -------------------------------------------------------------

void AnimationManagerWindow::TryLoad()
{
    if (m_loaded) return;
    m_loaded = true;

    std::string path = m_folderPath;
    if (!path.empty() && path.back() != '\\' && path.back() != '/')
        path += '\\';
    path += "moveset.anmbin";

    m_anmbin = LoadAnmbin(path);
    if (!m_anmbin.loaded && m_anmbin.errorMsg.empty())
        m_anmbin.errorMsg = "File not found: " + path;
}

// -------------------------------------------------------------
//  ForceReload
//  Called by MovesetEditorWindow after auto-rebuilding the anmbin.
//  Resets loaded state so next Render() re-reads moveset.anmbin from disk.
// -------------------------------------------------------------

void AnimationManagerWindow::ForceReload()
{
    m_loaded   = false;
    m_mapBuilt = false;
    for (int c = 0; c < 6; ++c)
        m_poolFilenames[c].clear();
    // Keep m_selRow / m_pendingTab so the UI stays on the same position.
}

// -------------------------------------------------------------
//  SetMotbinAnimKeys / BuildAnimKeyMap
// -------------------------------------------------------------

void AnimationManagerWindow::SetMotbinAnimKeys(const std::vector<uint32_t>& animKeys)
{
    m_motbinAnimKeys = animKeys;
    m_mapBuilt       = false;
}

void AnimationManagerWindow::BuildAnimKeyMap()
{
    if (m_mapBuilt) return;
    TryLoad();
    if (!m_anmbin.loaded) return;

    m_hashToAnimKey.clear();
    for (int cat = 0; cat < 6; ++cat)
        m_animKeyToPoolIdx[cat].clear();

    // Map anmbin moveList hashes → motbin anim_keys (original animations).
    if (!m_motbinAnimKeys.empty())
    {
        for (int cat = 0; cat < 6; ++cat)
        {
            const auto& ml = m_anmbin.moveList[cat];
            for (int i = 0; i < (int)ml.size() && i < (int)m_motbinAnimKeys.size(); ++i)
                m_hashToAnimKey.emplace(ml[i], m_motbinAnimKeys[i]);
        }
    }

    for (int cat = 0; cat < 6; ++cat)
    {
        const auto& pool = m_anmbin.pool[cat];
        for (int j = 0; j < (int)pool.size(); ++j)
        {
            uint32_t hash = static_cast<uint32_t>(pool[j].animKey & 0xFFFFFFFF);
            auto it = m_hashToAnimKey.find(hash);
            if (it != m_hashToAnimKey.end())
                m_animKeyToPoolIdx[cat].emplace(it->second, j);
        }
    }

    // Pass B: DoRefresh entries in memory (animDataPtr == 1 sentinel).
    // CRC32 == motbin_anim_key for these entries (not yet in anmbin moveList).
    for (int cat = 0; cat < 6; ++cat)
    {
        for (const auto& kv : m_poolFilenames[cat])
        {
            int idx = kv.first;
            if (idx >= (int)m_anmbin.pool[cat].size()) continue;
            uint32_t crc32 = static_cast<uint32_t>(m_anmbin.pool[cat][idx].animKey & 0xFFFFFFFF);
            if (crc32 == 0) continue;
            m_hashToAnimKey.emplace(crc32, crc32);   // for display
            m_animKeyToPoolIdx[cat].emplace(crc32, idx);
        }
    }

    // Pass C: Saved CRC32-based user animations (post-rebuild, animDataPtr is real PANM offset).
    // After reopen, m_poolFilenames is empty but animNameDB still has the CRC32 → name mapping.
    // These entries are identified by animNameDB recognizing pool[j].animKey (low32) as a key
    // (possible only for user-added CRC32-style entries, not original game entries whose
    // animKey is a game-internal hash never stored in animNameDB).
    if (m_animNameDB)
    {
        for (int cat = 0; cat < 6; ++cat)
        {
            const auto& pool = m_anmbin.pool[cat];
            for (int j = 0; j < (int)pool.size(); ++j)
            {
                if (pool[j].animDataPtr == 0 || pool[j].animDataPtr == 1) continue;
                uint32_t h = static_cast<uint32_t>(pool[j].animKey & 0xFFFFFFFF);
                if (m_animNameDB->AnimKeyToName(h).empty()) continue;
                // CRC32 == motbin_key for this saved user entry.
                m_animKeyToPoolIdx[cat].emplace(h, j);
                m_hashToAnimKey.emplace(h, h); // for display
            }
        }
    }

    m_mapBuilt = true;
}

// -------------------------------------------------------------
//  Lookup API
//  Naming: "anim_N" where N = pool index (animDataPtr != 0)
//           "com_N" where N = pool index (animDataPtr == 0)
//  N is the POOL INDEX directly — matches extraction tool file naming convention.
// -------------------------------------------------------------

std::string AnimationManagerWindow::AnimKeyToName(uint32_t motbinAnimKey, int cat)
{
    BuildAnimKeyMap();
    if (cat < 0 || cat >= 6) return "";
    auto it = m_animKeyToPoolIdx[cat].find(motbinAnimKey);
    if (it == m_animKeyToPoolIdx[cat].end()) return "";
    int poolIdx = it->second;
    const auto& pool = m_anmbin.pool[cat];
    if (poolIdx >= (int)pool.size()) return "";
    if (pool[poolIdx].animDataPtr == 0) return ""; // com ref — no name, use raw key
    char buf[64];
    if (!m_charaCode.empty())
        snprintf(buf, sizeof(buf), "anim_%s_%d", m_charaCode.c_str(), poolIdx);
    else
        snprintf(buf, sizeof(buf), "anim_%d", poolIdx);
    return buf;
}

bool AnimationManagerWindow::NameToAnimKey(const std::string& name, uint32_t& outMotbinKey, int cat)
{
    BuildAnimKeyMap();
    if (cat < 0 || cat >= 6) return false;

    if (name.rfind("anim_", 0) != 0) return false;
    size_t u = name.rfind('_');
    int N = (u != std::string::npos) ? std::atoi(name.c_str() + u + 1) : -1;
    if (N < 0) return false;

    const auto& pool = m_anmbin.pool[cat];
    if (N >= (int)pool.size()) return false;
    if (pool[N].animDataPtr == 0) return false; // com ref — not addressable by name

    uint32_t hash = static_cast<uint32_t>(pool[N].animKey & 0xFFFFFFFF);
    auto it = m_hashToAnimKey.find(hash);
    if (it == m_hashToAnimKey.end()) return false;

    outMotbinKey = it->second;
    return true;
}

int AnimationManagerWindow::AnimKeyToPoolIdx(uint32_t motbinAnimKey, int cat)
{
    BuildAnimKeyMap();
    if (cat < 0 || cat >= 6) return -1;
    auto it = m_animKeyToPoolIdx[cat].find(motbinAnimKey);
    return it != m_animKeyToPoolIdx[cat].end() ? it->second : -1;
}

void AnimationManagerWindow::NavigateToPool(int cat, int poolIdx)
{
    TryLoad();
    if (!m_anmbin.loaded) return;
    if (cat < 0 || cat >= 6) return;
    if (poolIdx < 0 || poolIdx >= (int)m_anmbin.pool[cat].size()) return;
    m_selRow[cat]        = poolIdx;
    m_pendingTab         = cat;
    m_scrollPending[cat] = true;
}

std::vector<AnimationManagerWindow::NewAnimEntry>
AnimationManagerWindow::TakePendingNewEntries()
{
    std::vector<NewAnimEntry> result;
    result.swap(m_pendingNew);
    return result;
}

// -------------------------------------------------------------
//  DoRefresh
// -------------------------------------------------------------

static uint32_t LocalCRC32(const std::string& path)
{
    static uint32_t table[256];
    static bool init = false;
    if (!init)
    {
        for (uint32_t i = 0; i < 256; ++i)
        {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) return 0;
    uint32_t crc = 0xFFFFFFFFu;
    uint8_t  buf[4096];
    size_t   n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        for (size_t i = 0; i < n; ++i)
            crc = table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    fclose(f);
    return crc ^ 0xFFFFFFFFu;
}

void AnimationManagerWindow::DoRefresh()
{
    TryLoad();

    m_lastRefresh = {};
    m_lastRefresh.ran = true;

    if (!m_anmbin.loaded)
    {
        m_lastRefresh.statusLines = "ERROR: moveset.anmbin not loaded.";
        return;
    }

    std::string base = m_folderPath;
    if (!base.empty() && base.back() != '\\' && base.back() != '/') base += '\\';

    char lineBuf[512];
    int  totalFound = 0;

    for (int cat = 0; cat < 6; ++cat)
    {
        std::string catDir  = base + "anim\\" + AnmbinCategoryFolder(cat);
        std::string ext     = AnmbinCategoryExt(cat);
        std::string pattern = catDir + "\\*" + ext;

        WIN32_FIND_DATAA fd = {};
        HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE)
        {
            snprintf(lineBuf, sizeof(lineBuf),
                     "[%s] folder not found: %s\n",
                     AnmbinCategoryName(cat), catDir.c_str());
            m_lastRefresh.statusLines += lineBuf;
            continue;
        }

        int existCount = (int)m_anmbin.pool[cat].size();

        // Build CRC32 set from pool animKeys (catches user-previously-added files
        // whose CRC32 was stored as animKey after rebuild).
        std::unordered_set<uint32_t> knownCRC32s;
        for (int j = 0; j < existCount; ++j)
            knownCRC32s.insert(static_cast<uint32_t>(m_anmbin.pool[cat][j].animKey & 0xFFFFFFFF));

        // Count existing local (non-com) entries for type-local index assignment.
        int animTypeIdx = 0;
        for (int j = 0; j < existCount; ++j)
            if (m_anmbin.pool[cat][j].animDataPtr != 0) ++animTypeIdx;

        struct FileCandidate { std::string filename; uint32_t hash; };
        std::vector<FileCandidate> candidates;
        int skippedPattern = 0;
        int skippedCRC     = 0;

        do {
            std::string fname = fd.cFileName;

            // CHECK 0: filename matches pool-index naming convention (anim_<code>_N or com_N).
            // Only treat as "extracted original" when the charcode prefix matches this
            // moveset's charcode.  A file like "anim_grf_500.bin" in Law's folder has the
            // same pool index (500) as Law's original but is a completely different animation;
            // it must NOT be skipped here — it should be picked up by the CRC check below.
            {
                std::string stem = fname;
                size_t sdot = stem.rfind('.');
                if (sdot != std::string::npos) stem = stem.substr(0, sdot);
                int  N       = -1;
                bool wantCom = false;

                // Build the expected prefix for this moveset's extracted originals.
                // e.g. "anim_law_" if charaCode="law", or just "anim_" for legacy/no-code.
                if (stem.rfind("anim_", 0) == 0)
                {
                    std::string expected = m_charaCode.empty()
                        ? std::string("anim_")
                        : std::string("anim_") + m_charaCode + "_";
                    if (stem.rfind(expected, 0) == 0) // prefix matches this moveset
                    {
                        size_t u = stem.rfind('_');
                        N = (u != std::string::npos) ? atoi(stem.c_str() + u + 1) : -1;
                    }
                    wantCom = false;
                }
                else if (stem.rfind("com_", 0) == 0)
                {
                    size_t u = stem.rfind('_');
                    N = (u != std::string::npos) ? atoi(stem.c_str() + u + 1) : -1;
                    wantCom = true;
                }
                if (N >= 0 && N < existCount)
                {
                    bool isCom = (m_anmbin.pool[cat][N].animDataPtr == 0);
                    if (isCom == wantCom)
                    {
                        // Extracted original: pool slot N already covers this file.
                        ++skippedPattern;
                        continue;
                    }
                }
            }

            std::string fpath = catDir + "\\" + fname;
            uint32_t    hash  = LocalCRC32(fpath);
            if (hash == 0) continue;

            // CHECK 3: CRC32 already a pool animKey (file was previously embedded in anmbin).
            if (knownCRC32s.count(hash))
            { ++skippedCRC; continue; }

            candidates.push_back({ fname, hash });
        } while (FindNextFileA(h, &fd));

        FindClose(h);

        snprintf(lineBuf, sizeof(lineBuf),
                 "[%s] %d new  |  %d skipped(pool-name)  |  %d skipped(CRC)\n"
                 "  path: %s\n",
                 AnmbinCategoryName(cat),
                 (int)candidates.size(), skippedPattern, skippedCRC,
                 catDir.c_str());
        m_lastRefresh.statusLines += lineBuf;

        if (candidates.empty()) continue;

        std::sort(candidates.begin(), candidates.end(),
            [](const FileCandidate& a, const FileCandidate& b) {
                return a.filename < b.filename;
            });

        int nextIdx = existCount;

        for (const auto& c : candidates)
        {
            int idx = nextIdx++;

            while ((int)m_anmbin.pool[cat].size() <= idx)
            {
                AnmbinEntry blank = {};
                m_anmbin.pool[cat].push_back(blank);
            }
            m_anmbin.pool[cat][idx].animKey     = static_cast<uint64_t>(c.hash);
            // Sentinel (non-zero) so entry is classified as LOCAL, not com ref.
            m_anmbin.pool[cat][idx].animDataPtr = 1;

            if (idx + 1 > (int)m_anmbin.poolCounts[cat])
                m_anmbin.poolCounts[cat] = static_cast<uint32_t>(idx + 1);

            m_mapBuilt = false;

            // Name = filename stem.  This matches the extraction tool convention:
            // "anim_500.bin" → stem "anim_500" = the name used in the editor and json.
            std::string stem = c.filename;
            size_t dot = stem.rfind('.');
            if (dot != std::string::npos) stem = stem.substr(0, dot);

            m_poolFilenames[cat][idx] = stem;

            snprintf(lineBuf, sizeof(lineBuf),
                     "  -> \"%s\"  name=%s  (CRC32=0x%08X  pool[%d])\n",
                     c.filename.c_str(), stem.c_str(), c.hash, idx);
            m_lastRefresh.statusLines += lineBuf;

            NewAnimEntry e;
            e.cat      = cat;
            e.poolIdx  = idx;
            e.hash     = c.hash;
            e.name     = stem;
            e.filename = c.filename;
            m_pendingNew.push_back(e);
            ++totalFound;
        }
    }

    m_lastRefresh.totalFound = totalFound;
    if (m_lastRefresh.statusLines.empty())
        m_lastRefresh.statusLines = "No categories found.";
}

// -------------------------------------------------------------
//  NavigateByMotbinKey
//  Navigate to the pool entry for a given motbin anim_key.
//  Works for both original animations (via map) and newly-added
//  animations not yet in moveList (direct CRC32 pool scan).
// -------------------------------------------------------------

void AnimationManagerWindow::NavigateByMotbinKey(int cat, uint32_t motbinAnimKey)
{
    TryLoad();
    if (!m_anmbin.loaded) return;
    if (cat < 0 || cat >= 6) return;
    BuildAnimKeyMap();

    // Primary: map lookup (covers both original and DoRefresh entries after BuildAnimKeyMap fix).
    int poolIdx = AnimKeyToPoolIdx(motbinAnimKey, cat);

    // Fallback: direct scan (in case map is stale or entry is missing).
    if (poolIdx < 0)
    {
        const auto& pool = m_anmbin.pool[cat];
        for (int j = 0; j < (int)pool.size(); ++j)
        {
            if ((pool[j].animKey & 0xFFFFFFFF) == motbinAnimKey)
            { poolIdx = j; break; }
        }
    }

    if (poolIdx >= 0)
        NavigateToPool(cat, poolIdx);
}

// -------------------------------------------------------------
//  NavigateTo
// -------------------------------------------------------------

void AnimationManagerWindow::NavigateTo(int cat, int moveIdx)
{
    TryLoad();
    if (!m_anmbin.loaded) return;
    if (cat < 0 || cat >= 6) return;

    const auto& ml = m_anmbin.moveList[cat];
    if (moveIdx < 0 || moveIdx >= (int)ml.size()) return;

    uint32_t targetHash = ml[moveIdx];
    const auto& pool = m_anmbin.pool[cat];
    for (int i = 0; i < (int)pool.size(); ++i)
    {
        if ((pool[i].animKey & 0xFFFFFFFF) == targetHash)
        {
            m_selRow[cat]        = i;
            m_pendingTab         = cat;
            m_scrollPending[cat] = true;
            return;
        }
    }
}

// -------------------------------------------------------------
//  RenderTabContent
//  Display: com.anmbin entries first (com_0, com_1, ...),
//           then local entries (anim_0, anim_1, ...).
// -------------------------------------------------------------

void AnimationManagerWindow::RenderTabContent(int cat)
{
    const auto& pool = m_anmbin.pool[cat];

    if (pool.empty())
    {
        ImGui::TextDisabled("(no entries)");
        return;
    }

    int animCount = 0;
    for (const auto& e : pool)
        if (e.animDataPtr != 0) ++animCount;

    ImGui::TextDisabled("%d animations", animCount);
    ImGui::Separator();

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.f, 2.f));
    if (ImGui::BeginTable("##anm_list", 2,
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit,
        ImVec2(0.f, 0.f)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        // Name = filename stem = pool-index name (anim_N / com_N where N = pool index).
        // These two are identical for extraction-tool-named files.
        ImGui::TableSetupColumn("Name (= File stem)", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Motbin Key",         ImGuiTableColumnFlags_WidthFixed, 100.f);
        ImGui::TableHeadersRow();

        int& sel      = m_selRow[cat];
        bool doScroll = m_scrollPending[cat];
        m_scrollPending[cat] = false;

        // Helper: get display name for pool entry i.
        // Priority: (1) DoRefresh filename stem, (2) animNameDB lookup, (3) pool-index fallback.
        auto getEntryName = [&](int i) -> std::string {
            // DoRefresh entries have their filename stem stored explicitly.
            auto fit = m_poolFilenames[cat].find(i);
            if (fit != m_poolFilenames[cat].end()) return fit->second;

            uint32_t hash32 = static_cast<uint32_t>(pool[i].animKey & 0xFFFFFFFF);

            // Look up via motbin key map → animNameDB (original entries).
            auto hit = m_hashToAnimKey.find(hash32);
            if (hit != m_hashToAnimKey.end() && m_animNameDB)
            {
                std::string n = m_animNameDB->AnimKeyToName(hit->second);
                if (!n.empty()) return n;
            }

            // Direct animNameDB lookup for saved CRC32-based user animations after reopen.
            // For these entries, pool[i].animKey (low32) == CRC32 == motbin_key stored in DB.
            if (m_animNameDB)
            {
                std::string n = m_animNameDB->AnimKeyToName(hash32);
                if (!n.empty()) return n;
            }

            // Fallback: pool-index name.
            char buf[64];
            bool isCom = (pool[i].animDataPtr == 0);
            if (!m_charaCode.empty() && !isCom)
                snprintf(buf, sizeof(buf), "anim_%s_%d", m_charaCode.c_str(), i);
            else
                snprintf(buf, sizeof(buf), isCom ? "com_%d" : "anim_%d", i);
            return buf;
        };

        // Helper: display motbin key for a pool entry.
        // - DoRefresh sentinels (animDataPtr==1) and saved CRC32 user entries:
        //   pool[i].animKey (low32) == CRC32 == motbin_key → show CRC32 directly.
        // - Original game entries: use m_hashToAnimKey lookup.
        auto showMotbinKey = [&](int i, bool grey) {
            uint32_t hash32 = static_cast<uint32_t>(pool[i].animKey & 0xFFFFFFFF);

            // DoRefresh sentinel: CRC32 IS the motbin key.
            if (m_poolFilenames[cat].count(i))
            {
                grey ? ImGui::TextDisabled("0x%08X", hash32) : ImGui::Text("0x%08X", hash32);
                return;
            }

            // Original entry: look up via moveList hash mapping.
            auto it = m_hashToAnimKey.find(hash32);
            if (it != m_hashToAnimKey.end())
            {
                grey ? ImGui::TextDisabled("0x%08X", it->second) : ImGui::Text("0x%08X", it->second);
                return;
            }

            // Saved CRC32-based user entry (post-rebuild reopen): animNameDB has the key.
            if (m_animNameDB && !m_animNameDB->AnimKeyToName(hash32).empty())
            {
                grey ? ImGui::TextDisabled("0x%08X", hash32) : ImGui::Text("0x%08X", hash32);
                return;
            }

            // No mapping found.
            ImGui::TextDisabled(grey ? "-" : "(no mapping)");
        };

        // anim_ entries only (animDataPtr != 0); com refs are hidden.
        for (int i = 0; i < (int)pool.size(); ++i)
        {
            if (pool[i].animDataPtr == 0) continue;
            bool selected = (sel == i);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            std::string entName = getEntryName(i);
            ImGui::PushID(i);
            if (ImGui::Selectable(entName.c_str(), selected,
                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
                ImVec2(0.f, 0.f)))
                sel = i;
            if (doScroll && selected) ImGui::SetScrollHereY(0.5f);
            ImGui::TableSetColumnIndex(1);
            showMotbinKey(i, false);
            ImGui::PopID();
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

// -------------------------------------------------------------
//  Render
// -------------------------------------------------------------

bool AnimationManagerWindow::Render()
{
    if (!m_open) return false;

    TryLoad();
    BuildAnimKeyMap();

    ImGui::SetNextWindowSize(ImVec2(560.f, 500.f), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse;

    if (!ImGui::Begin(m_windowId.c_str(), &m_open, flags))
    {
        ImGui::End();
        return m_open;
    }

    if (!m_anmbin.loaded)
    {
        ImGui::TextColored(ImVec4(1.f, 0.35f, 0.35f, 1.f),
                           "%s", m_anmbin.errorMsg.c_str());
        if (ImGui::SmallButton("Copy"))
            ImGui::SetClipboardText(m_anmbin.errorMsg.c_str());
        ImGui::TextDisabled("Extract moveset.anmbin first, then re-open.");
        ImGui::End();
        return m_open;
    }

    // --- Toolbar ---
    if (ImGui::SmallButton("Refresh from disk"))
        DoRefresh();

    // --- Rebuild error (set by MovesetEditorWindow if RebuildAnmbin failed) ---
    if (!m_rebuildError.empty())
    {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "Rebuild FAILED: %s", m_rebuildError.c_str());
        if (ImGui::SmallButton("Dismiss##rberr")) m_rebuildError.clear();
    }

    // --- Refresh result ---
    if (m_lastRefresh.ran)
    {
        ImGui::Separator();
        if (m_lastRefresh.totalFound > 0)
            ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f),
                               "Refresh: %d new animation(s) found. Rebuilding anmbin...",
                               m_lastRefresh.totalFound);
        else
            ImGui::TextDisabled("Refresh: no new files found.");

        if (ImGui::TreeNode("Details##refresh_log"))
        {
            ImGui::TextUnformatted(m_lastRefresh.statusLines.c_str());
            ImGui::TreePop();
        }
    }
    else
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(place .<ext> in anim/<cat>/, then Refresh)");
    }

    ImGui::Separator();

    if (ImGui::BeginTabBar("##anm_tabs"))
    {
        for (int cat = 0; cat < 6; ++cat)
        {
            char tabLabel[40];
            int localCount = 0;
            for (const auto& e : m_anmbin.pool[cat])
                if (e.animDataPtr != 0) ++localCount;
            snprintf(tabLabel, sizeof(tabLabel), "%s (%d)##cat%d",
                     AnmbinCategoryName(cat), localCount, cat);

            ImGuiTabItemFlags tabFlags = (m_pendingTab == cat)
                                         ? ImGuiTabItemFlags_SetSelected
                                         : ImGuiTabItemFlags_None;

            if (ImGui::BeginTabItem(tabLabel, nullptr, tabFlags))
            {
                if (m_pendingTab == cat) m_pendingTab = -1;
                RenderTabContent(cat);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
    return m_open;
}
