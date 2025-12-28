#include "UI/MessageBoxComponent.h"

namespace toy::kit {

MessageBoxComponent::MessageBoxComponent(class toy::Actor* owner, int drawOrder,  toy::VisualLayer layer)
: toy::TextSpriteComponent(owner, drawOrder, layer)
{
    
}

void MessageBoxComponent::SetTextBoxSize(const Vector2& size)
{
    
}

void MessageBoxComponent::SetPadding(const Vector2& padding)
{
    
}

void MessageBoxComponent::SetMessage(const std::string& text)
{
}

bool MessageBoxComponent::HasNextPage() const
{
    
}
void MessageBoxComponent::NextPage()
{
    
}
void MessageBoxComponent::ResetPage()
{
    
}

const std::string& MessageBoxComponent::GetCurrentPageText() const
{
    
}

void MessageBoxComponent::BuildPages()
{
    
}


} // namespace toy::kit
