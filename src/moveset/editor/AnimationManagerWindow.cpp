// AnimationManagerWindow.cpp
#include "AnimationManagerWindow.h"
#include "moveset/data/AnmbinRebuild.h"
#include "imgui/imgui.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>

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
// -------------------------------------------------------------

void AnimationManagerWindow::ForceReload()
{
    m_loaded   = false;
    m_mapBuilt = false;
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

    // Pass A: Map anmbin moveList hashes → motbin anim_keys (original animations).
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

    // Pass B: Saved CRC32-based user animations (post-rebuild reopen).
    // pool[j].animKey (low32) == CRC32 == motbin_key stored in animNameDB.
    if (m_animNameDB)
    {
        for (int cat = 0; cat < 6; ++cat)
        {
            const auto& pool = m_anmbin.pool[cat];
            for (int j = 0; j < (int)pool.size(); ++j)
            {
                if (pool[j].animDataPtr == 0) continue;
                uint32_t h = static_cast<uint32_t>(pool[j].animKey & 0xFFFFFFFF);
                if (m_animNameDB->AnimKeyToName(h).empty()) continue;
                m_animKeyToPoolIdx[cat].emplace(h, j);
                m_hashToAnimKey.emplace(h, h);
            }
        }
    }

    m_mapBuilt = true;
}

// -------------------------------------------------------------
//  Lookup API
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
    if (pool[poolIdx].animDataPtr == 0) return "";
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
    if (pool[N].animDataPtr == 0) return false;
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

void AnimationManagerWindow::NavigateByMotbinKey(int cat, uint32_t motbinAnimKey)
{
    TryLoad();
    if (!m_anmbin.loaded) return;
    if (cat < 0 || cat >= 6) return;
    BuildAnimKeyMap();

    int poolIdx = AnimKeyToPoolIdx(motbinAnimKey, cat);

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
//  ComputePanmSize
//  Determines the byte size of a PANM blob by sorting all pool
//  entries across all cats by animDataPtr and computing the gap.
// -------------------------------------------------------------

size_t AnimationManagerWindow::ComputePanmSize(int cat, int poolIdx)
{
    TryLoad();
    if (!m_anmbin.loaded) return 0;
    if (cat < 0 || cat >= 6) return 0;
    if (poolIdx < 0 || poolIdx >= (int)m_anmbin.pool[cat].size()) return 0;

    uint64_t targetPtr = m_anmbin.pool[cat][poolIdx].animDataPtr;
    if (targetPtr == 0) return 0;

    // Collect all non-zero animDataPtrs across all cats
    std::vector<uint64_t> ptrs;
    for (int c = 0; c < 6; ++c)
        for (const auto& e : m_anmbin.pool[c])
            if (e.animDataPtr != 0) ptrs.push_back(e.animDataPtr);
    std::sort(ptrs.begin(), ptrs.end());
    ptrs.erase(std::unique(ptrs.begin(), ptrs.end()), ptrs.end());

    // Get file size
    std::string anmbinPath = m_folderPath;
    if (!anmbinPath.empty() && anmbinPath.back() != '\\' && anmbinPath.back() != '/')
        anmbinPath += '\\';
    anmbinPath += "moveset.anmbin";

    FILE* f = nullptr;
    fopen_s(&f, anmbinPath.c_str(), "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    size_t fileSize = static_cast<size_t>(ftell(f));
    fclose(f);

    auto it = std::lower_bound(ptrs.begin(), ptrs.end(), targetPtr);
    if (it == ptrs.end()) return 0;

    auto next = std::next(it);
    if (next != ptrs.end())
        return static_cast<size_t>(*next - *it);
    else
        return fileSize - static_cast<size_t>(*it);
}

// -------------------------------------------------------------
//  DoAdd  --  open file dialog, read bytes, embed into anmbin
// -------------------------------------------------------------

void AnimationManagerWindow::DoAdd(int cat)
{
    if (!m_moves || !m_animNameDB) { m_statusMsg = "Not ready (no moves data)"; m_statusOk = false; return; }

    // Open file dialog
    const char* filter = (cat == 0)
        ? "Animation (*.bin)\0*.bin\0All files (*.*)\0*.*\0\0"
        : "All files (*.*)\0*.*\0\0";
    char filePath[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = NULL;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile   = filePath;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameA(&ofn)) return; // cancelled

    // Read file bytes
    FILE* f = nullptr;
    if (fopen_s(&f, filePath, "rb") != 0 || !f)
    { m_statusMsg = "Cannot open file"; m_statusOk = false; return; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); m_statusMsg = "Empty file"; m_statusOk = false; return; }
    std::vector<uint8_t> panmBytes(static_cast<size_t>(sz));
    fread(panmBytes.data(), 1, panmBytes.size(), f);
    fclose(f);

    // Extract stem name from filename
    std::string stem = filePath;
    {
        size_t p = stem.rfind('\\');
        if (p == std::string::npos) p = stem.rfind('/');
        if (p != std::string::npos) stem = stem.substr(p + 1);
        size_t d = stem.rfind('.');
        if (d != std::string::npos) stem = stem.substr(0, d);
    }

    uint32_t crc32 = 0;
    std::string err;
    bool ok = AddAnimToAnmbin(m_folderPath, *m_animNameDB, *m_moves, cat, panmBytes, crc32, err);

    if (ok)
    {
        if (m_onAnimAdded) m_onAnimAdded(cat, stem, crc32);
        char msg[128];
        snprintf(msg, sizeof(msg), "Added: %s  (0x%08X)", stem.c_str(), crc32);
        m_statusMsg = msg;
        m_statusOk  = true;
        ForceReload();
    }
    else
    {
        m_statusMsg = "Add failed: " + err;
        m_statusOk  = false;
    }
}

// -------------------------------------------------------------
//  DoExtract  --  read PANM bytes from anmbin, save to file
// -------------------------------------------------------------

void AnimationManagerWindow::DoExtract(int cat, int poolIdx)
{
    TryLoad();
    if (!m_anmbin.loaded) return;
    if (poolIdx < 0 || poolIdx >= (int)m_anmbin.pool[cat].size()) return;

    uint64_t animDataPtr = m_anmbin.pool[cat][poolIdx].animDataPtr;
    if (animDataPtr == 0)
    { m_statusMsg = "Cannot extract: com.anmbin reference"; m_statusOk = false; return; }

    size_t panmSize = ComputePanmSize(cat, poolIdx);
    if (panmSize == 0)
    { m_statusMsg = "Cannot determine animation size"; m_statusOk = false; return; }

    // Build default save filename
    char defName[MAX_PATH] = {};
    snprintf(defName, sizeof(defName), "anim_%d%s", poolIdx, AnmbinCategoryExt(cat));

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = NULL;
    ofn.lpstrFilter = "Animation file\0*.*\0\0";
    ofn.lpstrFile   = defName;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameA(&ofn)) return; // cancelled

    // Read PANM bytes from anmbin
    std::string anmbinPath = m_folderPath;
    if (!anmbinPath.empty() && anmbinPath.back() != '\\' && anmbinPath.back() != '/')
        anmbinPath += '\\';
    anmbinPath += "moveset.anmbin";

    FILE* f = nullptr;
    if (fopen_s(&f, anmbinPath.c_str(), "rb") != 0 || !f)
    { m_statusMsg = "Cannot read moveset.anmbin"; m_statusOk = false; return; }
    fseek(f, static_cast<long>(animDataPtr), SEEK_SET);
    std::vector<uint8_t> panmBytes(panmSize);
    fread(panmBytes.data(), 1, panmSize, f);
    fclose(f);

    // Write to selected path
    FILE* out = nullptr;
    if (fopen_s(&out, defName, "wb") != 0 || !out)
    { m_statusMsg = "Cannot write output file"; m_statusOk = false; return; }
    fwrite(panmBytes.data(), 1, panmBytes.size(), out);
    fclose(out);

    m_statusMsg = std::string("Extracted to: ") + defName;
    m_statusOk  = true;
}

// -------------------------------------------------------------
//  DoRemove  --  remove pool entry from anmbin, notify motbin
// -------------------------------------------------------------

void AnimationManagerWindow::DoRemove(int cat, int poolIdx)
{
    uint32_t removedHash = 0;
    std::string err;
    bool ok = RemoveAnimFromAnmbin(m_folderPath, cat, poolIdx, removedHash, err);

    if (ok)
    {
        if (m_onAnimRemoved) m_onAnimRemoved(removedHash);
        char msg[80];
        snprintf(msg, sizeof(msg), "Removed pool[%d][%d]  (key 0x%08X)", cat, poolIdx, removedHash);
        m_statusMsg = msg;
        m_statusOk  = true;
        // Clamp selection
        if (m_selRow[cat] >= poolIdx && m_selRow[cat] > 0) --m_selRow[cat];
        ForceReload();
    }
    else
    {
        m_statusMsg = "Remove failed: " + err;
        m_statusOk  = false;
    }
}

// -------------------------------------------------------------
//  DoExtractAll  --  dump all pool entries to a chosen folder
// -------------------------------------------------------------

void AnimationManagerWindow::DoExtractAll()
{
    TryLoad();
    if (!m_anmbin.loaded) return;

    // Open Vista-style folder picker (IFileDialog)
    char selectedPath[MAX_PATH] = {};
    {
        IFileDialog* pfd = nullptr;
        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&pfd))))
            return;

        DWORD opts = 0;
        pfd->GetOptions(&opts);
        pfd->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        pfd->SetTitle(L"Select output folder for animation export");

        if (SUCCEEDED(pfd->Show(NULL)))
        {
            IShellItem* psi = nullptr;
            if (SUCCEEDED(pfd->GetResult(&psi)))
            {
                PWSTR pszPath = nullptr;
                if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)))
                {
                    WideCharToMultiByte(CP_ACP, 0, pszPath, -1,
                                        selectedPath, MAX_PATH, nullptr, nullptr);
                    CoTaskMemFree(pszPath);
                }
                psi->Release();
            }
        }
        pfd->Release();
        if (selectedPath[0] == '\0') return; // user cancelled
    }

    std::string destRoot = selectedPath;
    if (destRoot.empty())
    { m_statusMsg = "No output folder selected"; m_statusOk = false; return; }
    if (destRoot.back() != '\\' && destRoot.back() != '/') destRoot += '\\';

    // Read anmbin into memory once
    std::string anmbinPath = m_folderPath;
    if (!anmbinPath.empty() && anmbinPath.back() != '\\' && anmbinPath.back() != '/')
        anmbinPath += '\\';
    anmbinPath += "moveset.anmbin";

    FILE* f = nullptr;
    if (fopen_s(&f, anmbinPath.c_str(), "rb") != 0 || !f)
    { m_statusMsg = "Cannot open moveset.anmbin"; m_statusOk = false; return; }
    fseek(f, 0, SEEK_END);
    size_t fileSize = static_cast<size_t>(ftell(f));
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> anmbinBytes(fileSize);
    fread(anmbinBytes.data(), 1, fileSize, f);
    fclose(f);

    // Collect and sort all (animDataPtr, cat, poolIdx) tuples for size computation
    struct Entry { uint64_t ptr; int cat; int idx; };
    std::vector<Entry> entries;
    for (int c = 0; c < 6; ++c)
        for (int j = 0; j < (int)m_anmbin.pool[c].size(); ++j)
            if (m_anmbin.pool[c][j].animDataPtr != 0)
                entries.push_back({ m_anmbin.pool[c][j].animDataPtr, c, j });
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b){ return a.ptr < b.ptr; });

    int exported = 0;
    for (int i = 0; i < (int)entries.size(); ++i)
    {
        size_t panmSize = (i + 1 < (int)entries.size())
            ? static_cast<size_t>(entries[i+1].ptr - entries[i].ptr)
            : fileSize - static_cast<size_t>(entries[i].ptr);

        if (entries[i].ptr + panmSize > fileSize) continue;

        int c   = entries[i].cat;
        int idx = entries[i].idx;

        // Build output path: <destRoot>/<cat_folder>/<anim_N><ext>
        std::string catDir = destRoot + AnmbinCategoryFolder(c);
        CreateDirectoryA(catDir.c_str(), NULL);

        char fname[MAX_PATH];
        snprintf(fname, sizeof(fname), "%s\\anim_%d%s",
                 catDir.c_str(), idx, AnmbinCategoryExt(c));

        FILE* out = nullptr;
        if (fopen_s(&out, fname, "wb") != 0 || !out) continue;
        fwrite(anmbinBytes.data() + entries[i].ptr, 1, panmSize, out);
        fclose(out);
        ++exported;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "Exported %d animations to: %s", exported, destRoot.c_str());
    m_statusMsg = msg;
    m_statusOk  = true;
}

// -------------------------------------------------------------
//  RenderTabContent  --  left panel list for one category tab
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
    for (const auto& e : pool) if (e.animDataPtr != 0) ++animCount;
    ImGui::TextDisabled("%d animations", animCount);
    ImGui::Separator();

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.f, 2.f));
    if (ImGui::BeginTable("##anm_list", 3,
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit,
        ImVec2(0.f, 0.f)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name",     ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Anim_Key", ImGuiTableColumnFlags_WidthFixed, 110.f);
        ImGui::TableSetupColumn("Size",     ImGuiTableColumnFlags_WidthFixed,  72.f);
        ImGui::TableHeadersRow();

        int& sel      = m_selRow[cat];
        bool doScroll = m_scrollPending[cat];
        m_scrollPending[cat] = false;

        // Helper: display name for pool entry i
        auto getEntryName = [&](int i) -> std::string {
            uint32_t hash32 = static_cast<uint32_t>(pool[i].animKey & 0xFFFFFFFF);
            auto hit = m_hashToAnimKey.find(hash32);
            if (hit != m_hashToAnimKey.end() && m_animNameDB)
            {
                std::string n = m_animNameDB->AnimKeyToName(hit->second);
                if (!n.empty()) return n;
            }
            if (m_animNameDB)
            {
                std::string n = m_animNameDB->AnimKeyToName(hash32);
                if (!n.empty()) return n;
            }
            char buf[64];
            bool isCom = (pool[i].animDataPtr == 0);
            if (!m_charaCode.empty() && !isCom)
                snprintf(buf, sizeof(buf), "anim_%s_%d", m_charaCode.c_str(), i);
            else
                snprintf(buf, sizeof(buf), isCom ? "com_%d" : "anim_%d", i);
            return buf;
        };

        // Pre-compute PANM sizes for this category (one file-open for the whole list)
        // Build sorted ptr list + fileSize once, then map ptr→size per entry
        std::vector<size_t> panmSizes(pool.size(), 0);
        {
            std::vector<uint64_t> allPtrs;
            for (int c = 0; c < 6; ++c)
                for (const auto& e : m_anmbin.pool[c])
                    if (e.animDataPtr != 0) allPtrs.push_back(e.animDataPtr);
            std::sort(allPtrs.begin(), allPtrs.end());
            allPtrs.erase(std::unique(allPtrs.begin(), allPtrs.end()), allPtrs.end());

            size_t fileSize = 0;
            {
                std::string ap = m_folderPath;
                if (!ap.empty() && ap.back() != '\\' && ap.back() != '/') ap += '\\';
                ap += "moveset.anmbin";
                FILE* f = nullptr;
                fopen_s(&f, ap.c_str(), "rb");
                if (f) { fseek(f, 0, SEEK_END); fileSize = (size_t)ftell(f); fclose(f); }
            }

            for (int i = 0; i < (int)pool.size(); ++i)
            {
                uint64_t ptr = pool[i].animDataPtr;
                if (ptr == 0) continue;
                auto it = std::lower_bound(allPtrs.begin(), allPtrs.end(), ptr);
                if (it == allPtrs.end()) continue;
                auto nx = std::next(it);
                panmSizes[i] = (nx != allPtrs.end())
                    ? (size_t)(*nx - *it)
                    : fileSize - (size_t)*it;
            }
        }

        // Helper: Anim_Key display
        // For Fullbody (cat==0): show real motbin key; others: "unknown"
        auto showAnimKey = [&](int i) {
            if (cat != 0) { ImGui::TextDisabled("unknown"); return; }
            uint32_t hash32 = static_cast<uint32_t>(pool[i].animKey & 0xFFFFFFFF);
            auto it = m_hashToAnimKey.find(hash32);
            if (it != m_hashToAnimKey.end())
            { ImGui::Text("0x%08X", it->second); return; }
            if (m_animNameDB && !m_animNameDB->AnimKeyToName(hash32).empty())
            { ImGui::Text("0x%08X", hash32); return; }
            ImGui::TextDisabled("-");
        };

        for (int i = 0; i < (int)pool.size(); ++i)
        {
            if (pool[i].animDataPtr == 0) continue; // skip com refs

            bool selected = (sel == i);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            std::string entName = getEntryName(i);
            ImGui::PushID(i);

            if (ImGui::Selectable(entName.c_str(), selected,
                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
                ImVec2(0.f, 0.f)))
                sel = i;

            // Right-click context menu
            if (ImGui::BeginPopupContextItem("##row_ctx"))
            {
                sel = i;
                if (ImGui::MenuItem("Extract"))
                    DoExtract(cat, i);
                ImGui::Separator();
                if (ImGui::MenuItem("Remove"))
                {
                    m_removeConfirm.showing  = true;
                    m_removeConfirm.cat      = cat;
                    m_removeConfirm.poolIdx  = i;
                    m_removeConfirm.animName = entName;
                }
                ImGui::EndPopup();
            }

            if (doScroll && selected) ImGui::SetScrollHereY(0.5f);

            ImGui::TableSetColumnIndex(1);
            showAnimKey(i);

            ImGui::TableSetColumnIndex(2);
            if (panmSizes[i] > 0)
            {
                char szBuf[32];
                if (panmSizes[i] >= 1024)
                    snprintf(szBuf, sizeof(szBuf), "%zu KB", panmSizes[i] / 1024);
                else
                    snprintf(szBuf, sizeof(szBuf), "%zu B", panmSizes[i]);
                ImGui::Text("%s", szBuf);
            }
            else
                ImGui::TextDisabled("-");

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

// -------------------------------------------------------------
//  RenderPreviewPanel  --  right panel (3D preview placeholder)
// -------------------------------------------------------------

void AnimationManagerWindow::RenderPreviewPanel()
{
    float panelH = ImGui::GetContentRegionAvail().y;
    float previewH  = panelH * 0.72f;
    float controlH  = panelH - previewH - ImGui::GetStyle().ItemSpacing.y;

    // 3D preview area
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.07f, 0.07f, 1.f));
    if (ImGui::BeginChild("##3d_preview", ImVec2(0.f, previewH), true))
    {
        float cw = ImGui::GetContentRegionAvail().x;
        float ch = ImGui::GetContentRegionAvail().y;
        const char* line1 = "3D Preview";
        const char* line2 = "not yet implemented";
        float lh = ImGui::GetTextLineHeightWithSpacing();
        ImGui::SetCursorPos(ImVec2(
            (cw - ImGui::CalcTextSize(line1).x) * 0.5f,
            ch * 0.5f - lh));
        ImGui::TextDisabled("%s", line1);
        ImGui::SetCursorPos(ImVec2(
            (cw - ImGui::CalcTextSize(line2).x) * 0.5f,
            ch * 0.5f));
        ImGui::TextDisabled("%s", line2);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Control panel
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.f));
    if (ImGui::BeginChild("##ctrl_panel", ImVec2(0.f, controlH), true))
    {
        ImGui::BeginDisabled();
        if (ImGui::Button("|<")) {} ImGui::SameLine();
        if (ImGui::Button(" < ")) {} ImGui::SameLine();
        if (ImGui::Button(" > ")) {} ImGui::SameLine();
        if (ImGui::Button("> |")) {}
        float dummy = 0.f;
        ImGui::SetNextItemWidth(-1.f);
        ImGui::SliderFloat("##frame_slider", &dummy, 0.f, 100.f, "Frame: 0");
        ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// -------------------------------------------------------------
//  Render
// -------------------------------------------------------------

bool AnimationManagerWindow::Render()
{
    if (!m_open) return false;

    TryLoad();
    BuildAnimKeyMap();

    ImGui::SetNextWindowSize(ImVec2(1200.f, 600.f), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse;

    if (!ImGui::Begin(m_windowId.c_str(), &m_open, flags))
    {
        ImGui::End();
        return m_open;
    }

    if (!m_anmbin.loaded)
    {
        ImGui::TextColored(ImVec4(1.f, 0.35f, 0.35f, 1.f), "%s", m_anmbin.errorMsg.c_str());
        if (ImGui::SmallButton("Copy"))
            ImGui::SetClipboardText(m_anmbin.errorMsg.c_str());
        ImGui::TextDisabled("Extract the moveset first, then re-open.");
        ImGui::End();
        return m_open;
    }

    // ── Toolbar ──────────────────────────────────────────────────────────────

    // [Add ▾] button
    if (ImGui::Button("Add"))
        ImGui::OpenPopup("##add_menu");

    if (ImGui::BeginPopup("##add_menu"))
    {
        if (ImGui::MenuItem("Fullbody"))  { DoAdd(0); ImGui::CloseCurrentPopup(); }
        ImGui::BeginDisabled();
        ImGui::MenuItem("Hand");
        ImGui::MenuItem("Facial");
        ImGui::MenuItem("Swing");
        ImGui::MenuItem("Camera");
        ImGui::MenuItem("Extra");
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    // [Extract All Animation] button
    if (ImGui::Button("Extract All Animation"))
        DoExtractAll();

    // Status message
    if (!m_statusMsg.empty())
    {
        ImGui::SameLine();
        ImVec4 col = m_statusOk
            ? ImVec4(0.35f, 1.f, 0.45f, 1.f)
            : ImVec4(1.f,   0.4f, 0.4f,  1.f);
        ImGui::TextColored(col, "%s", m_statusMsg.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("x##clrsts")) m_statusMsg.clear();
    }

    // Rebuild error
    if (!m_rebuildError.empty())
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "Rebuild FAILED: %s", m_rebuildError.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Dismiss##rberr")) m_rebuildError.clear();
    }

    ImGui::Separator();

    // ── Tabs + split layout ───────────────────────────────────────────────────

    if (ImGui::BeginTabBar("##anm_tabs"))
    {
        for (int cat = 0; cat < 6; ++cat)
        {
            int localCount = 0;
            for (const auto& e : m_anmbin.pool[cat])
                if (e.animDataPtr != 0) ++localCount;

            char tabLabel[40];
            snprintf(tabLabel, sizeof(tabLabel), "%s (%d)##cat%d",
                     AnmbinCategoryName(cat), localCount, cat);

            ImGuiTabItemFlags tabFlags = (m_pendingTab == cat)
                ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;

            if (ImGui::BeginTabItem(tabLabel, nullptr, tabFlags))
            {
                if (m_pendingTab == cat) m_pendingTab = -1;

                float availW = ImGui::GetContentRegionAvail().x;
                float leftW  = availW * 0.60f;
                float rightW = availW - leftW - ImGui::GetStyle().ItemSpacing.x;

                if (ImGui::BeginChild("##list_panel", ImVec2(leftW, 0.f), false))
                    RenderTabContent(cat);
                ImGui::EndChild();

                ImGui::SameLine();

                if (ImGui::BeginChild("##preview_panel", ImVec2(rightW, 0.f), false))
                    RenderPreviewPanel();
                ImGui::EndChild();

                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    // ── Remove confirmation modal ─────────────────────────────────────────────

    if (m_removeConfirm.showing)
    {
        ImGui::OpenPopup("Confirm Remove##anmmgr");
        m_removeConfirm.showing = false;
    }

    if (ImGui::BeginPopupModal("Confirm Remove##anmmgr", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Remove animation from anmbin?");
        ImGui::Text("  %s  (pool[%d][%d])",
                    m_removeConfirm.animName.c_str(),
                    m_removeConfirm.cat,
                    m_removeConfirm.poolIdx);
        ImGui::TextColored(ImVec4(1.f,0.8f,0.3f,1.f),
            "Moves referencing this animation will have their anim_key cleared.");
        ImGui::Spacing();
        if (ImGui::Button("Remove", ImVec2(100.f, 0.f)))
        {
            DoRemove(m_removeConfirm.cat, m_removeConfirm.poolIdx);
            m_removeConfirm = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100.f, 0.f)))
        {
            m_removeConfirm = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
    return m_open;
}
