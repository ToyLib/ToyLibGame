#pragma once

#include "Engine/Debug/DebugStats.h"

#include <SDL3/SDL.h>
#include <vector>
#include <memory>
#include <string>

namespace toy {

class Application
{
public:
    Application();
    virtual ~Application();
    
    // アプリ全体の初期化（SDL, Renderer, 各Subsystem 初期化）
    bool Initialize();
    
    // メインループ（ProcessInput → UpdateFrame → Draw）
    void RunLoop();
    
    // 全解放
    void Shutdown();
    
    //-----------------------------------------
    // Actor 管理
    //-----------------------------------------
    void AddActor(std::unique_ptr<class Actor> a);
    
    template <typename T, typename... Args>
    T* CreateActor(Args&&... args)
    {
        auto actor = std::make_unique<T>(this, std::forward<Args>(args)...);
        T* rawPtr = actor.get();
        AddActor(std::move(actor));
        return rawPtr;
    }
    
    void DestroyActor(class Actor* actor);
    
    //-----------------------------------------
    // システム取得
    //-----------------------------------------
    class Renderer*        GetRenderer()        const { return mRenderer.get(); }
    class PhysWorld*       GetPhysWorld()       const { return mPhysWorld.get(); }
    class AssetManager*    GetAssetManager()    const { return mAssetManager.get(); }
    class AssetManager*    GetSysAssetManager() const { return mSystemAssetManager.get(); }
    class SoundMixer*      GetSoundMixer()      const { return mSoundMixer.get(); }
    class InputSystem*     GetInputSystem()     const { return mInputSys.get(); }
    class TimeOfDaySystem* GetTimeOfDaySystem() const { return mTimeOfDaySys.get(); }
    
    float GetTimeSconds() const { return SDL_GetTicks() * 0.001f; }
    
    //-----------------------------------------
    // ウィンドウ操作
    //-----------------------------------------
    bool IsFullScreen() const { return mIsFullScreen; }
    void SetFullscreen(bool enable);
    void ToggleFullscreen();
    
    //-----------------------------------------
    // デバッグ情報
    //-----------------------------------------
    DebugStats&       GetDebugStats()       { return mDebugStats; }
    const DebugStats& GetDebugStats() const { return mDebugStats; }

    
protected:
    //-----------------------------------------
    // ゲーム側でオーバーライドするフック関数
    //-----------------------------------------
    virtual void UpdateGame(float deltaTime) { }
    virtual void InitGame() {}
    virtual void ShutdownGame() {}
    virtual void ProcessInput(const struct InputState& input) {}

    void InitAssetManager(const std::string& path, float dpi = 1.0f);
    
private:
    //-----------------------------------------
    // 内部処理（ゲームループ関連）
    //-----------------------------------------
    void LoadData();
    void UnloadData();
    void ProcessInput();
    void UpdateFrame();
    void Draw();
    
    //-----------------------------------------
    // ウィンドウ／アプリ設定
    //-----------------------------------------
    std::string mApplicationTitle; // ウィンドウタイトル
    bool  mIsFullScreen;           // フルスクリーン状態

    // 現在のウィンドウの「物理解像度」（描画用）
    int   mScreenWidth;
    int   mScreenHeight;

    SDL_Window* mWindow;           // SDLウィンドウ

    // ウィンドウモード時の「論理ウィンドウサイズ」（復帰用）
    int   mWindowedWidth;
    int   mWindowedHeight;

    bool  mIsActive;
    bool  mIsPause;
    Uint64 mTicksCount;            // 前フレームの時刻（ns 単位）

    // ウィンドウ操作関連ヘルパー
    void HandleWindowResized();
    
    // アスペクト比固定関連
    float mTargetAspect;          // 幅 / 高さ
    bool  mLockAspect;            // アスペクトロック有効か
    bool  mIsAdjustingSize;       // 自前でサイズ変更中か（イベントループ防止）
    
    //-----------------------------------------
    // サブシステム
    //-----------------------------------------
    std::unique_ptr<class Renderer>        mRenderer;
    std::unique_ptr<class InputSystem>     mInputSys;
    std::unique_ptr<class PhysWorld>       mPhysWorld;
    std::unique_ptr<class AssetManager>    mAssetManager;
    std::unique_ptr<class SoundMixer>      mSoundMixer;
    std::unique_ptr<class TimeOfDaySystem> mTimeOfDaySys;

    std::unique_ptr<class AssetManager>    mSystemAssetManager; // システム用:非公開
    friend class DebugOverlayActor;
    
    //-----------------------------------------
    // Actor 管理
    //-----------------------------------------
    std::vector<std::unique_ptr<class Actor>> mActors;
    std::vector<std::unique_ptr<class Actor>> mPendingActors;
    bool mIsUpdatingActors;
    
    //-----------------------------------------
    // デバッグ情報
    //-----------------------------------------
    DebugStats mDebugStats;
    
    //-----------------------------------------
    // 設定読み込み
    //-----------------------------------------
    bool LoadSettings(const std::string& filePath);
};

} // namespace toy
