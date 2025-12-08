#include "FieldScene.h"
#include "KitCore/Game.h"
#include "KitCore/Stage.h"
#include "KitCore/Character.h"

#include "Utils/MathUtil.h"   // Vector3



using namespace toy::kit;

FieldScene::FieldScene()
: mStage(nullptr)    // Stage は Game* を後からセットするため
{
}

void FieldScene::OnEnter()
{
    Game* game = GetGame();      // Scene → Game（Applicationの派生）

    // Stage を初期化（Game 参照を渡す）
    mStage = Stage(game);

    // JSON などからステージ構成を読み込む
    mStage.LoadFromFile("Settings/Stages/field01.json");

    // プレイヤー生成
    //toy::Actor* playerActor = mStage.spawnPlayer("hero");
    //mPlayer = Character(playerActor);

    // 敵配置（仮のハードコード）
    //mStage.spawnEnemy("slime", Vector3(2.0f, 0.0f, 3.0f));
    //mStage.spawnEnemy("slime", Vector3(-3.0f, 0.0f, 4.0f));

    // カメラ・HUD の初期化を追加する場合はここ
    // mCamera.Follow(mPlayer);
}

void FieldScene::OnExit()
{
    // Stage の担当エンティティを破棄依頼
    mStage.Unload();

    // 必要ならカメラ / HUD なども破棄
}

void FieldScene::Update(float deltaTime)
{
    // プレイヤー操作
    //mPlayer.Update(deltaTime);

    // ステージ内のギミック・敵 AI の更新などもここで書ける
    mStage.Update(deltaTime);

    // カメラを追従させるならここ
    // mCamera.Update(deltaTime);
}

