// RenderBackendState.h
#pragma once
#include <cstdint>

namespace toy {

enum class RenderBackendType : uint8_t
{
    Unknown = 0,
    OpenGL,
    Vulkan
};

class RenderBackendState
{
public:
    static RenderBackendState& Get()
    {
        static RenderBackendState s;
        return s;
    }

    // --- 問い合わせ ---
    RenderBackendType Type() const { return mType; }
    bool IsGL() const { return mType == RenderBackendType::OpenGL; }
    bool IsVK() const { return mType == RenderBackendType::Vulkan; }

    // --- 設定（Application だけで呼ぶ想定） ---
    void Set(RenderBackendType t) { mType = t; }

private:
    RenderBackendType mType = RenderBackendType::Unknown;
};

} // namespace toy
