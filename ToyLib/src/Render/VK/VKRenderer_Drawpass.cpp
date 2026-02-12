#include "Render/VK/VKRenderer.h"

namespace toy {


void VKRenderer::DrawItem(const RenderItem& it, RenderPass pass, int cascadeIndex)
{
    const PipelineHandle ph = it.pipeline;
    
    if (!ph.IsValidVK())
    {
        return;
    }
    
    auto* p = reinterpret_cast<VKPipeline*>(ph.ptrVKPipeline);
    vkCmdBindPipeline(mFrames[mFrameIndex].cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p->pipeline);
    
    // 以降：DS bind / VB bind / draw...
}

} // namespace toy
