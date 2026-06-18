// TkmodManagerView.cpp
#include "fbsdata/editor/TkmodManagerView.h"
#include "fbsdata/editor/FbsDataView.h"
#include "fbsdata/io/TkmodIO.h"
#include "Config.h"
#include "imgui/imgui.h"
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <algorithm>
#include <functional>

// -------------------------------------------------------------
//  String helpers
// -------------------------------------------------------------

static std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(len > 0 ? len - 1 : 0, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
    return w;
}

static std::string WideToUtf8(const wchar_t* w)
{
    if (!w || !*w) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(len > 0 ? len - 1 : 0, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], len, nullptr, nullptr);
    return s;
}

// -------------------------------------------------------------
//  Open / BrowseDirectory / ScanAndLoad
// -------------------------------------------------------------

void TkmodManagerView::Open()
{
    m_open       = true;
    m_firstFrame = true;
    m_dir = Config::Get().data.tkmodManagerDir;
    if (!m_dir.empty()) ScanAndLoad();
}

void TkmodManagerView::BrowseDirectory()
{
    IFileDialog* pfd = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&pfd))))
        return;

    DWORD opts = 0;
    pfd->GetOptions(&opts);
    pfd->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

    if (SUCCEEDED(pfd->Show(nullptr)))
    {
        IShellItem* psi = nullptr;
        if (SUCCEEDED(pfd->GetResult(&psi)))
        {
            PWSTR path = nullptr;
            if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &path)))
            {
                m_dir = WideToUtf8(path);
                CoTaskMemFree(path);
            }
            psi->Release();
        }
    }
    pfd->Release();

    if (!m_dir.empty())
    {
        Config::Get().data.tkmodManagerDir = m_dir;
        Config::Get().Save();
        ScanAndLoad();
    }
}

void TkmodManagerView::ScanAndLoad()
{
    m_files.clear();
    m_binNames.clear();
    m_selectedBinNameIdx = -1;

    if (m_dir.empty()) return;

    std::wstring wDir = Utf8ToWide(m_dir);
    if (!wDir.empty() && wDir.back() != L'\\' && wDir.back() != L'/')
        wDir += L'\\';

    // Recursive directory scan via std::function lambda
    std::function<void(const std::wstring&)> scan = [&](const std::wstring& dir)
    {
        WIN32_FIND_DATAW fd = {};

        // Collect .tkmod files in this directory
        HANDLE hFiles = FindFirstFileW((dir + L"*.tkmod").c_str(), &fd);
        if (hFiles != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                TkmodFile tf;
                tf.filename = WideToUtf8(fd.cFileName);
                tf.path     = WideToUtf8((dir + fd.cFileName).c_str());
                TkmodIO::LoadFromPath(tf.path, tf.data);
                m_files.push_back(std::move(tf));
            }
            while (FindNextFileW(hFiles, &fd));
            FindClose(hFiles);
        }

        // Recurse into subdirectories
        HANDLE hDirs = FindFirstFileW((dir + L"*").c_str(), &fd);
        if (hDirs == INVALID_HANDLE_VALUE) return;
        do
        {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            scan(dir + fd.cFileName + L"\\");
        }
        while (FindNextFileW(hDirs, &fd));
        FindClose(hDirs);
    };

    scan(wDir);

    // Build unique bin names (preserve first-seen order)
    for (const auto& tf : m_files)
    {
        for (const auto& bin : tf.data.contents)
        {
            if (std::find(m_binNames.begin(), m_binNames.end(), bin.name) == m_binNames.end())
                m_binNames.push_back(bin.name);
        }
    }
}

// -------------------------------------------------------------
//  RenderContents -- right panel: unique bin type list
// -------------------------------------------------------------

void TkmodManagerView::RenderContents(float listW)
{
    ImGui::SetCursorPos(ImVec2(10.0f, 8.0f));

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.82f, 1.00f, 1.00f));
    ImGui::Text("Contents");
    ImGui::PopStyleColor();

    ImGui::SameLine(listW - 60.0f);
    if (ImGui::SmallButton("Reload"))
        ScanAndLoad();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);

    if (m_binNames.empty())
    {
        ImGui::TextDisabled(m_dir.empty() ? "Set a directory first." : "No .tkmod files found.");
        return;
    }

    const float itemH  = 32.0f;
    const float availH = ImGui::GetContentRegionAvail().y;
    ImGui::BeginChild("##MgrBinList", ImVec2(0.0f, availH), false);

    for (int i = 0; i < (int)m_binNames.size(); ++i)
    {
        const bool selected = (i == m_selectedBinNameIdx);
        ImGui::PushID(i);

        if (selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.22f, 0.40f, 0.72f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.28f, 0.48f, 0.82f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0.25f, 0.44f, 0.78f, 1.00f));
        }

        if (ImGui::Selectable(m_binNames[i].c_str(), selected,
                              ImGuiSelectableFlags_None, ImVec2(0.0f, itemH)))
            m_selectedBinNameIdx = i;

        if (selected)
            ImGui::PopStyleColor(3);

        ImGui::PopID();
    }

    ImGui::EndChild();
}

// -------------------------------------------------------------
//  RenderEditor -- left panel: merged data from all tkmods
// -------------------------------------------------------------

void TkmodManagerView::RenderEditor(FbsDataView& fbsView,
                                     std::function<void(const std::string&)>& cb)
{
    if (m_selectedBinNameIdx < 0 || m_selectedBinNameIdx >= (int)m_binNames.size())
    {
        const char* hint = "Select a bin type from the Contents list.";
        const float cw = ImGui::GetContentRegionAvail().x;
        const float ch = ImGui::GetContentRegionAvail().y;
        ImGui::SetCursorPos(ImVec2((cw - ImGui::CalcTextSize(hint).x) * 0.5f, ch * 0.45f));
        ImGui::TextDisabled("%s", hint);
        return;
    }

    const std::string& binName = m_binNames[m_selectedBinNameIdx];

    std::vector<FbsDataView::BinViewSource> sources;
    for (auto& tf : m_files)
    {
        for (auto& b : tf.data.contents)
        {
            if (b.name == binName)
            {
                sources.push_back({ tf.filename.c_str(), tf.path.c_str(), &b, &tf.data });
                break;
            }
        }
    }

    if (sources.empty())
    {
        ImGui::TextDisabled("No tkmod files contain this bin.");
        return;
    }

    ImGui::BeginChild("##MgrEditorScroll", ImVec2(0.0f, 0.0f), false);
    fbsView.RenderBinMergedOverview(sources, cb);
    ImGui::EndChild();
}

// -------------------------------------------------------------
//  Render -- top-level window
// -------------------------------------------------------------

void TkmodManagerView::Render(FbsDataView& fbsView,
                               std::function<void(const std::string&)> openInEditorCb)
{
    if (!m_open) return;

    if (m_firstFrame)
    {
        ImGuiViewport* mv = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(mv->Pos.x + mv->Size.x * 0.1f, mv->Pos.y + 60.0f),
                                ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(1200.0f, 750.0f), ImGuiCond_Always);
        m_firstFrame = false;
    }

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse;

    bool open = true;
    if (!ImGui::Begin("tkmod Overview", &open, kFlags))
    {
        ImGui::End();
        m_open = open;
        return;
    }
    m_open = open;

    // Toolbar
    if (ImGui::Button("Set tkmod Directory"))
        BrowseDirectory();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_dir.empty() ? "(not set)" : m_dir.c_str());
    ImGui::Separator();

    const float totalH = ImGui::GetContentRegionAvail().y;
    const float totalW = ImGui::GetContentRegionAvail().x;
    constexpr float LIST_W = 290.0f;
    const float editorW = totalW - LIST_W - 1.0f;

    // Editor area (left)
    ImGui::BeginChild("##mgr_editor", ImVec2(editorW, totalH), false,
                      ImGuiWindowFlags_NoScrollbar);
    RenderEditor(fbsView, openInEditorCb);
    ImGui::EndChild();

    ImGui::SameLine(0, 0);

    // 1px divider
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.20f, 0.20f, 0.28f, 1.00f));
    ImGui::BeginChild("##mgr_div", ImVec2(1.0f, totalH), false);
    ImGui::PopStyleColor();
    ImGui::EndChild();

    ImGui::SameLine(0, 0);

    // Contents list (right)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.09f, 0.12f, 1.00f));
    ImGui::BeginChild("##mgr_list", ImVec2(LIST_W, totalH), false,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();
    RenderContents(LIST_W);
    ImGui::EndChild();

    ImGui::End();
}
