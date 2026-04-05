#pragma once
#include "MovesetExtractor.h"
#include <string>
#include <functional>
#include <thread>
#include <atomic>

// -------------------------------------------------------------
//  ExtractorView
//  Two-button panel: "Extract P1" / "Extract P2"
//  Each button auto-connects, refreshes slot info, and extracts.
//  Extraction runs on a worker thread; call CheckThread() each
//  frame before Render* so results are applied on the main thread.
// -------------------------------------------------------------
class ExtractorView {
public:
    explicit ExtractorView(const std::string& movesetRootDir);

    // Render only the Extract P1 / Extract P2 buttons.
    void RenderButtons();

    // Render the status log, slot info, and save-path hint.
    void RenderLog();

    // Update the destination folder (called when config changes).
    void SetDestFolder(const std::string& dir) { m_destFolder = dir; }

    // Called after a successful extraction so the caller can
    // trigger a moveset list refresh.
    void SetOnExtractSuccess(std::function<void()> cb) { m_onSuccess = std::move(cb); }

    // Returns true while the worker thread is running.
    bool IsLoading() const { return m_loading; }

    // Call once per frame (before rendering) to pick up completed results.
    void CheckThread();

private:
    // Pre-check + start worker thread.
    void StartExtract(int slotIndex);

    MovesetExtractor      m_extractor;
    std::string           m_destFolder;
    std::string           m_lastMsg;
    bool                  m_lastOk  = false;
    std::function<void()> m_onSuccess;

    // Async state
    bool              m_loading    = false;
    std::thread       m_thread;
    std::atomic<bool> m_threadDone { false };
    std::string       m_threadMsg;
    bool              m_threadOk   = false;
};
