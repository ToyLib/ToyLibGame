#pragma once

#include "ToyKit.h"
#include "ToyLib.h"

#include <memory>
#include <vector>

class FieldScene : public toy::kit::IScene
{
public:
    explicit FieldScene();

    // Player/FieldMonster は前方宣言のみのため、それらの std::unique_ptr を
    // 完全型が見える FieldScene.cpp 側で暗黙生成させる（out-of-line destructor）
    ~FieldScene() override;

    void ProcessInput(const struct toy::InputState& input) override;
    void Update(float deltaTime) override;
protected:
    // IScene の4フェーズ（Environment / World / SpawnCharacters / UI）
    void DefineEnvironment() override;
    void DefineWorld() override;
    void SpawnCharacters() override;
    void DefineUI() override;
private:

    void InitField();
    void DeployGround();
    void DeploySky();
    void DeployBricks();
    void DeployFire(Vector3 pos);
    std::unique_ptr<class toy::WeatherManager> mWeather;
    
    class toy::TextSpriteComponent* mTextComp;

    // Prefab を内包する Game Logic 側オブジェクト（toy::Actor は継承しないため、
    // Scene 側で寿命を管理する）
    std::unique_ptr<class Player> mPlayer;
    std::vector<std::unique_ptr<class Noriko>> mMonsters;
    std::vector<std::unique_ptr<class Ninja>>  mNinjas;

    // メッシュ+コライダーだけの静止物（設計方針の StaticObject Prefab）
    std::vector<std::unique_ptr<toy::kit::StaticObject>> mStaticObjects;
};
