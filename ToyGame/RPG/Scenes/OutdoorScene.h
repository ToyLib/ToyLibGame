#pragma once

#include "ToyKit.h"
#include "ToyLib.h"

#include <memory>
#include <vector>

//=============================================================================
// OutdoorScene
//  RPG フィールドシーン。IScene を継承し GameFlow で管理される。
//  KitGame の FieldScene と同じ形（InitScene → DefineEnvironment/World/UI +
//  Deploy* ヘルパー）で構成する。
//=============================================================================

class OutdoorScene : public toy::kit::IScene
{
public:
    // Hero/Shiro/Wolf/MagicBolt/HealBurst は前方宣言のみのため、それらの
    // std::unique_ptr を完全型が見える OutdoorScene.cpp 側で暗黙生成させる
    // （out-of-line constructor/destructor）
    OutdoorScene();
    ~OutdoorScene() override;

    void ProcessInput(const toy::InputState& input) override;
    void Update(float deltaTime) override;

protected:
    void InitScene() override;
    void UnloadScene() override;

private:
    // InitScene() の構成（Environment / World / UI を宣言する場所を分ける）
    void DefineEnvironment();
    void DefineWorld();
    void DefineUI();

    void InitField();
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

    // Prefab を内包する Game Logic 側オブジェクト（toy::Actor は継承しないため、
    // Scene 側で寿命を管理する）
    std::unique_ptr<class Hero>                mHero;
    std::unique_ptr<class Shiro>               mShiro;
    std::vector<std::unique_ptr<class Wolf>>   mWolves;

    // Hero が発動した魔法/回復エフェクト。Hero の Signal を受けて Scene が
    // 生成し、寿命切れ（IsExpired）を毎フレーム回収する。
    std::vector<std::unique_ptr<class MagicBolt>> mMagicBolts;
    std::vector<std::unique_ptr<class HealBurst>> mHealBursts;

    // Hero の位置を毎フレーム反映するマーカー Actor（FollowMoveComponent 等、
    // 本物の toy::Actor を要求するレガシー系との橋渡し用）
    toy::Actor* mHeroMarker = nullptr;

    // メッシュ+コライダーだけの静止物（設計方針の StaticObject Prefab）
    std::vector<std::unique_ptr<toy::kit::StaticObject>> mStaticObjects;
};
