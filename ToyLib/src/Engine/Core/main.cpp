#include "Engine/Core/ApplicationEntry.h"
#include "Engine/Core/Application.h"
#include "Engine/Runtime/SingleInstance.h"

int main(int argc, char** argv)
{
    //---------------------------------------------------------
    // シングルインスタンスチェック
    // ・アプリの多重起動を防ぐ
    // ・すでに起動中なら IsLocked() が false になる
    //---------------------------------------------------------
    toy::SingleInstance instance;
    if (!instance.IsLocked())
    {
        return 1;
    }
    //---------------------------------------------------------
    // ユーザーアプリ（Application 派生）を生成
    // ・CreateUserApplication() は TOYLIB_REGISTER_APP で実装される
    // ・ToyLib のメイン関数からゲーム実装を差し替える仕組み
    //---------------------------------------------------------
    std::unique_ptr<toy::Application> app = CreateUserApplication();

    //---------------------------------------------------------
    // 初期化 → メインループ → 終了処理
    //---------------------------------------------------------
    if (app->Initialize())
    {
        app->RunLoop();
        app->Shutdown();
        return 0;      // 正常終了
    }

    return 2;          // 初期化失敗
}
