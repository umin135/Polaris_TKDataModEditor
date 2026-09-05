#ifdef _DEBUG

#include "T7DumpConvertView.h"
#include "Config.h"
#include "extract/T7MovesetExtractor.h"
#include "moveset/data/T7AliasDict.h"
#include "imgui/imgui.h"

#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#include <cstring>
#include <string>

static std::string BrowseForT7Dump()
{
    IFileDialog* pfd = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&pfd))))
        return {};

    COMDLG_FILTERSPEC filters[] = {
        { L"T7 dump (*.bin)", L"*.bin" },
        { L"All files (*.*)",  L"*.*"   },
    };
    pfd->SetFileTypes(2, filters);
    pfd->SetTitle(L"Select T7DUMP01 dump (.bin)");

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

void T7DumpConvertView::Render()
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
    ImGui::TextUnformatted("[ DEBUG ] T7 Dump → Motbin");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled(
        "Load a T7DUMP01 .bin dumped from Tekken 7 memory and convert it to\n"
        "TK7_<Name>/moveset.motbin using the same T7→T8 path as live extract.\n"
        "Animations are not in the dump — anim keys use name hashes only.");
    ImGui::Spacing();

    const float btnW  = 80.0f;
    const float pathW = ImGui::GetContentRegionAvail().x
                      - btnW - ImGui::GetStyle().ItemSpacing.x;

    ImGui::TextUnformatted("T7 dump (.bin):");
    ImGui::SetNextItemWidth(pathW);
    ImGui::InputText("##dumpPath", m_dumpPath, sizeof(m_dumpPath));
    ImGui::SameLine();
    if (ImGui::Button("Browse##dump", ImVec2(btnW, 0.0f)))
    {
        std::string p = BrowseForT7Dump();
        if (!p.empty())
            strncpy_s(m_dumpPath, sizeof(m_dumpPath), p.c_str(), _TRUNCATE);
    }

    ImGui::Spacing();

    const std::string& rootDir = Config::Get().data.MovesetDir();
    if (rootDir.empty())
        ImGui::TextDisabled("Output: (set Game Root / Moveset Root in Settings first)");
    else
        ImGui::TextDisabled("Output: %s\\TK7_<Name>\\moveset.motbin", rootDir.c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextUnformatted("Aliases (t7_aliases.json):");
    if (ImGui::Button("Reload Aliases", ImVec2(160.0f, 0.0f)))
        ReloadAliases();
    ImGui::SameLine();
    ImGui::TextDisabled("prefers data\\MovesetDatas\\ over res\\");
    if (!m_aliasStatus.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text,
            m_aliasOk ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f)
                      : ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::TextWrapped("%s", m_aliasStatus.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const bool canRun = (m_dumpPath[0] != '\0') && !rootDir.empty();
    if (!canRun) ImGui::BeginDisabled();
    if (ImGui::Button("Convert Dump", ImVec2(160.0f, 0.0f)))
        RunConvert();
    if (!canRun) ImGui::EndDisabled();

    if (!canRun && rootDir.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("<- set Game Root in Settings");
    }

    if (!m_status.empty())
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text,
            m_statusOk ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f)
                       : ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::TextWrapped("%s", m_status.c_str());
        ImGui::PopStyleColor();
    }
}

void T7DumpConvertView::ReloadAliases()
{
    m_aliasStatus.clear();
    m_aliasOk = false;
    std::string path;
    if (!T7AliasDict::Get().ReloadFromDisk(&path)) {
        m_aliasStatus = "Reload failed — t7_aliases.json not found under data\\ or res\\.";
        return;
    }
    m_aliasOk = true;
    m_aliasStatus = "Reloaded: " + path;
}

void T7DumpConvertView::RunConvert()
{
    m_status.clear();
    m_statusOk = false;

    T7MovesetExtractor ex;
    std::string err;
    const std::string& dest = Config::Get().data.MovesetDir();
    if (!ex.ConvertDumpToFile(m_dumpPath, dest, err)) {
        m_status = err.empty() ? "Convert failed." : err;
        return;
    }
    m_statusOk = true;
    m_status = ex.GetStatusMsg();
}

#endif // _DEBUG
