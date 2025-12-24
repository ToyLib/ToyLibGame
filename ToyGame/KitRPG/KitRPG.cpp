#include "KitRPG.h"
#include "FieldScene.h"
#include "Engine/Core/ApplicationEntry.h"

// ToyLibの起動Applicationとして登録
//TOYLIB_REGISTER_APP(KitRPG)


KitRPG::KitRPG()
    : Game()
{
    InitAssetManager("GameApp/Assets/RPG/");
}

void KitRPG::Setup()
{
    //scenes().Change(std::make_unique<FieldScene>());
}

void KitRPG::Tick(float deltaTime)
{
    // RPG 全体での更新処理があればここ
}
