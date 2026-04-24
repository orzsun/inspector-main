#pragma once
#include "SkillMemoryReader.h"
#include <mutex>
#include <deque>

namespace kx {

class CaptureQueue {
public:
    static CaptureQueue& Instance() {
        static CaptureQueue inst;
        return inst;
    }

    void Push(const SkillbarSnapshot& s) {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_q.push_back(s);
    }

    bool TryPop(SkillbarSnapshot& out) {
        std::lock_guard<std::mutex> lk(m_mtx);
        if (m_q.empty()) return false;
        out = m_q.front();
        m_q.pop_front();
        return true;
    }

private:
    CaptureQueue() = default;
    ~CaptureQueue() = default;
    std::mutex m_mtx;
    std::deque<SkillbarSnapshot> m_q;
};

} // namespace kx
