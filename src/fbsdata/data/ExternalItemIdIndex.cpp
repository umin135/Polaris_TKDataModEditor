#include "fbsdata/data/ExternalItemIdIndex.h"
#include "fbsdata/data/ModData.h"
#include "fbsdata/io/TkmodIO.h"
#include <windows.h>
#include <functional>
#include <unordered_set>

ExternalItemIdIndex& ExternalItemIdIndex::Get()
{
    static ExternalItemIdIndex s_inst;
    return s_inst;
}

static std::wstring EII_Utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(len > 0 ? len - 1 : 0, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
    return w;
}

static std::string EII_WideToUtf8(const wchar_t* w)
{
    if (!w || !*w) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(len > 0 ? len - 1 : 0, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], len, nullptr, nullptr);
    return s;
}

void ExternalItemIdIndex::BuildFromDir(const std::string& dir)
{
    m_idToFiles.clear();
    m_built = true;               // "built" even if empty, so we don't retry
    if (dir.empty()) return;

    std::wstring wDir = EII_Utf8ToWide(dir);
    if (!wDir.empty() && wDir.back() != L'\\' && wDir.back() != L'/')
        wDir += L'\\';

    // Index one tkmod file: add each distinct item id it uses -> its filename.
    auto indexFile = [this](const std::string& path, const std::string& filename)
    {
        ModData data;
        if (!TkmodIO::LoadFromPath(path, data)) return;

        std::unordered_set<uint32_t> ids;   // distinct ids within this file
        for (const auto& bin : data.contents)
        {
            for (const auto& e : bin.commonEntries)              ids.insert(e.item_id);
            for (const auto& e : bin.customizeItemUniqueEntries) ids.insert(e.char_item_id);
        }
        for (uint32_t id : ids)
            m_idToFiles[id].push_back(filename);
    };

    // Recursive directory walk (mirrors TkmodManagerView::ScanAndLoad).
    std::function<void(const std::wstring&)> scan = [&](const std::wstring& d)
    {
        WIN32_FIND_DATAW fd = {};
        HANDLE hFiles = FindFirstFileW((d + L"*.tkmod").c_str(), &fd);
        if (hFiles != INVALID_HANDLE_VALUE)
        {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                indexFile(EII_WideToUtf8((d + fd.cFileName).c_str()),
                          EII_WideToUtf8(fd.cFileName));
            } while (FindNextFileW(hFiles, &fd));
            FindClose(hFiles);
        }

        HANDLE hDirs = FindFirstFileW((d + L"*").c_str(), &fd);
        if (hDirs == INVALID_HANDLE_VALUE) return;
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            scan(d + fd.cFileName + L"\\");
        } while (FindNextFileW(hDirs, &fd));
        FindClose(hDirs);
    };

    scan(wDir);
}

const char* ExternalItemIdIndex::FindOwner(uint32_t id, const std::string& selfFile) const
{
    auto it = m_idToFiles.find(id);
    if (it == m_idToFiles.end()) return nullptr;
    for (const std::string& f : it->second)
        if (f != selfFile) return f.c_str();   // first tkmod other than the edited one
    return nullptr;
}
