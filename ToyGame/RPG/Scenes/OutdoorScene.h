#pragma once

#include "ToyKit.h"
#include "ToyLib.h"

//=============================================================================
// OutdoorScene
//  RPG フィールドシーン。IScene を継承し GameFlow で管理される。
//
//  InitScene() の構成:
//    SetupEnvironment()  — ポストエフェクト / 時間帯 / BGM / 地面 / 空
//    SetupProps()        — 焚き火 / レンガ / 家 / 島 / 木 / 鏡
//    SetupCharacters()   — Hero / Wolf x5 / Shiro / Stan
//    SetupUI()           — HUD テキスト / ヘルスバー
//=============================================================================

class OutdoorScene : public toy::kit::IScene
{
public:
    OutdoorScene() = default;

    void ProcessInput(const toy::InputState& input) override;
    void Update(float deltaTime) override;

protected:
    void InitScene() override;
    void UnloadScene() override;

private:
    //------------------------------------------------------------------
    // 初期化サブルーチン
    //------------------------------------------------------------------
    void SetupEnvironment();
    void SetupProps();
    void SetupCharacters();
    void SetupUI();

    //------------------------------------------------------------------
    // Props ヘルパー
    //------------------------------------------------------------------
    void DeployGround();
    void DeploySky();
    void DeployFire(const Vector3& pos);
    void DeployBrick(const Vector3& pos);
    void DeployIsland(const Vector3& pos);
    void DeployHouse(const Vector3& pos);
    void DeployTree(const Vector3& pos);
    void DeployMirror(const Vector3& pos);

    //------------------------------------------------------------------
    // メンバー
    //------------------------------------------------------------------
    std::unique_ptr<toy::WeatherManager> mWeather;
    toy::TextSpriteComponent*            mTextComp = nullptr;
};
