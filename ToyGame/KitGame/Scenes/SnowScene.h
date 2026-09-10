#pragma once

#include "ToyKit.h"
#include "ToyLib.h"

#include <memory>
#include <vector>

class SnowScene : public toy::kit::IScene
{
public:
    explicit SnowScene();

    // Player/FieldMonster は前方宣言のみのため、それらの std::unique_ptr を
    // 完全型が見える SnowScene.cpp 側で暗黙生成させる（out-of-line destructor）
    ~SnowScene() override;

    void ProcessInput(const struct toy::InputState& input) override;
    void Update(float deltaTime) override;
protected:
    void InitScene() override;
private:
    // InitScene() の構成（Environment / World / UI を宣言する場所を分ける）
    void DefineEnvironment();
    void DefineWorld();
    void DefineUI();

    void InitField();
    void DeployGround();
    void DeploySky();
    void DeployFire(Vector3 pos);
    std::unique_ptr<class toy::WeatherManager> mWeather;

    class toy::TextSpriteComponent* mTextComp;

    toy::Actor* mPlyCamera;

    // Prefab を内包する Game Logic 側オブジェクト（toy::Actor は継承しないため、
    // Scene 側で寿命を管理する）
    std::unique_ptr<class Player> mPlayer;
    std::vector<std::unique_ptr<class FieldMonster>> mMonsters;
};
