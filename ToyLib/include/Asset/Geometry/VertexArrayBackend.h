#pragma once

namespace toy {

//-------------------------------------------
// IVertexArrayBackend
// ・GPU側（GL/VK）の VertexArray 実体
// ・VertexArray（CPU側）から Bind/Unload だけ呼ぶ
//-------------------------------------------
class IVertexArrayBackend
{
public:
    virtual ~IVertexArrayBackend() = default;

    virtual void Bind() = 0;
    virtual void Unload() = 0;
};

} // namespace toy
