#include "Environment/SkyDomeComponent.h"
#include "Environment/SkyDomeMeshGenerator.h"
#include "Asset/Geometry/VertexArray.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Render/LightingManager.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Application.h"
#include "Engine/Render/Renderer.h"
#include <algorithm>

namespace toy {

SkyDomeComponent::SkyDomeComponent(Actor* a)
    : Component(a)
{
    //========================================
    // ベースクラスとしての最低限の初期化
    //========================================

    // スカイドーム用メッシュ生成（半球）
    // 派生クラス（WeatherDomeComponent）がそのまま使う想定
    mSkyVAO = SkyDomeMeshGenerator::CreateSkyDomeVAO(32, 16, 1.0f);

    // Renderer に「スカイドームとして登録」
    // → Renderer側の描画パイプラインで SkyDomeComponent が呼ばれるようになる
    GetOwner()->GetApp()->GetRenderer()->RegisterSkyDome(this);

    // スカイドーム描画用の基本シェーダ取得
    // 派生クラスがここに Uniform を詰めて描画する
    mShader = GetOwner()->GetApp()->GetRenderer()->GetShader("SkyDome");
}

void SkyDomeComponent::Draw()
{
    //========================================
    // ベースクラスでは描画しない
    //========================================
    // WeatherDomeComponent が実際の描画処理を行うため、
    // ここでは VAO と Shader が揃っているか確認するだけ。
    
    if (!mSkyVAO || !mShader)
    {
        return;
    }
    // ※ここで描画内容を書かないのは「派生クラスに任せる」ため。
}

void SkyDomeComponent::Update(float deltaTime)
{
    //========================================
    // ベースクラスはロジックを持たない
    //========================================
    // 時間帯・天候・ライトカラーなどは WeatherDomeComponent で更新する。
}

} // namespace toy
