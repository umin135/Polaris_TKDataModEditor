#pragma once
#include "fbsdata/data/ModData.h"
#include <string>
#include <vector>
#include <functional>

class FbsDataView;

class TkmodManagerView
{
public:
    bool IsOpen() const { return m_open; }
    void Open();
    void Render(FbsDataView& fbsView, std::function<void(const std::string&)> openInEditorCb);

private:
    bool        m_open       = false;
    bool        m_firstFrame = false;
    std::string m_dir;

    struct TkmodFile { std::string path; std::string filename; ModData data; };
    std::vector<TkmodFile> m_files;

    std::vector<std::string> m_binNames;         // unique bin names across all files (first-seen order)
    int                      m_selectedBinNameIdx = -1;

    void ScanAndLoad();
    void BrowseDirectory();
    void RenderContents(float listW);
    void RenderEditor(FbsDataView& fbsView, std::function<void(const std::string&)>& cb);
};
