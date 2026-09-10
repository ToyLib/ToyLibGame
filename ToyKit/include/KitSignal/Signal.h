#pragma once

#include <functional>
#include <vector>

namespace toy::kit {

//=============================================================================
// Signal<TEvent>
//  Prefab が「事実」を通知し、Game Logic が Connect() で受け取るための
//  最小限の Signal/Handler 実装（設計方針 9）。
//=============================================================================
template<typename TEvent>
class Signal
{
public:
    using Handler = std::function<void(const TEvent&)>;

    void Connect(Handler handler) { mHandlers.push_back(std::move(handler)); }

    void Emit(const TEvent& e) const
    {
        for (const auto& h : mHandlers)
        {
            if (h) h(e);
        }
    }

private:
    std::vector<Handler> mHandlers;
};

} // namespace toy::kit
