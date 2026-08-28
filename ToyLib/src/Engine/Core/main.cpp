#include "Engine/Core/ApplicationEntry.h"
#include "Engine/Core/Application.h"
#include "Engine/Runtime/SingleInstance.h"

#ifdef _WIN32
#include <Windows.h>
#include <iostream>

// ドライバ内クラッシュ等、cerr を吐く間もなく落ちるケースを
// ログに残すための最終防衛ライン（例外コード/アドレスのみ記録）
static LONG WINAPI ToyLibUnhandledExceptionFilter(EXCEPTION_POINTERS* info)
{
    std::cerr << "[FATAL] Unhandled exception. Code=0x" << std::hex
              << info->ExceptionRecord->ExceptionCode
              << " Address=" << info->ExceptionRecord->ExceptionAddress
              << std::dec << std::endl;
    std::cerr.flush();
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int main(int argc, char** argv)
{
#ifdef _WIN32
    SetUnhandledExceptionFilter(ToyLibUnhandledExceptionFilter);
#endif

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
