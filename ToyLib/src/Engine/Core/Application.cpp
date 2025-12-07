#include "Engine/Core/Application.h"
#include "Engine/Core/Actor.h"
#include "Engine/Render/Renderer.h"
#include "Engine/Runtime/InputSystem.h"
#include "Physics/PhysWorld.h"
#include "Asset/AssetManager.h"
#include "Audio/SoundMixer.h"
#include "Engine/Runtime/TimeOfDaySystem.h"

#include <algorithm>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>

namespace toy {

//=============================================================
// コンストラクタ／デストラクタ
//=============================================================

Application::Application()
: mIsActive(false)
, mIsUpdatingActors(false)
, mIsPause(false)
, mScreenWidth(1600)    // 初期の想定物理解像度（あくまで暫定）
, mScreenHeight(900)
, mWindowedWidth(1280)  // 起動時の論理ウィンドウサイズ
, mWindowedHeight(768)
, mIsFullScreen(false)
, mWindow(nullptr)
, mTicksCount(0)
, mTargetAspect(16.0f / 9.0f)
, mLockAspect(true)
, mIsAdjustingSize(false)
{
    mRenderer      = std::make_unique<Renderer>();
    mInputSys      = std::make_unique<InputSystem>();
    mPhysWorld     = std::make_unique<PhysWorld>();
    mAssetManager  = std::make_unique<AssetManager>();
    mSoundMixer    = std::make_unique<SoundMixer>(mAssetManager.get());
    mTimeOfDaySys  = std::make_unique<TimeOfDaySystem>();
}

Application::~Application()
{
    // 実処理は Shutdown() 側で行う前提
}


//=============================================================
// 初期化／メインループ／終了処理
//=============================================================

bool Application::Initialize()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        std::cerr << "[Application] SDL_Init failed: "
                  << SDL_GetError() << std::endl;
        return false;
    }

    if (!TTF_Init())
    {
        std::cerr << "[Application] TTF_Init failed: "
                  << SDL_GetError() << std::endl;
        return false;
    }

    LoadSettings("ToyLib/Settings/Application_Settings.json");
    if (mScreenWidth  <= 0) mScreenWidth  = 1280;
    if (mScreenHeight <= 0) mScreenHeight = 720;

    // 起動時の論理ウィンドウサイズ
    mWindowedWidth  = mScreenWidth;
    mWindowedHeight = mScreenHeight;

    // DPI スケールを見て実際のウィンドウサイズを決める
    float contentScale = 1.0f;
    if (SDL_DisplayID primary = SDL_GetPrimaryDisplay())
    {
        float s = SDL_GetDisplayContentScale(primary);
        if (s > 0.0f)
        {
            contentScale = s;
        }
    }

    const int windowW = static_cast<int>(mWindowedWidth  * contentScale);
    const int windowH = static_cast<int>(mWindowedHeight * contentScale);

    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

    mWindow = SDL_CreateWindow(
        mApplicationTitle.c_str(),
        windowW,
        windowH,
        windowFlags
    );
    if (!mWindow)
    {
        std::cerr << "[Application] SDL_CreateWindow failed: "
                  << SDL_GetError() << std::endl;
        return false;
    }
    
    
    SDL_SetWindowPosition(mWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    // Renderer 初期化（GL コンテキスト生成など）
    if (!mRenderer->Initialize(mWindow))
    {
        std::cerr << "[Application] Renderer::Initialize failed." << std::endl;
        return false;
    }

    // 初期のウィンドウ物理解像度を取得し Renderer に通知
    HandleWindowResized();

    
    // 現在の論理サイズからアスペクト比を決める（16:9 ならほぼ 1.777... になる）
    int w = 0, h = 0;
    SDL_GetWindowSize(mWindow, &w, &h);
    if (w > 0 && h > 0)
    {
        mTargetAspect = static_cast<float>(w) / static_cast<float>(h);
    }
    
    // 入力システム初期化
    mInputSys->Initialize(mRenderer->GetSDLWindow());
    mInputSys->LoadButtonConfig("ToyLib/Settings/InputConfig.json");

    // 必要であれば起動時フルスクリーンに切り替え
    if (mIsFullScreen)
    {
        SetFullscreen(mIsFullScreen);
    }

    LoadData();
    InitGame();

    mIsActive   = true;
    mIsPause    = false;
    mTicksCount = SDL_GetTicksNS(); // ★ NS で統一

    return true;
}

void Application::RunLoop()
{
    while (mIsActive)
    {
        ProcessInput();
        UpdateFrame();
        Draw();
    }
}

void Application::Draw()
{
    mRenderer->Draw();
}

void Application::Shutdown()
{
    ShutdownGame();
    UnloadData();

    mInputSys->Shutdown();
    mRenderer->Shutdown();

    if (mWindow)
    {
        SDL_DestroyWindow(mWindow);
        mWindow = nullptr;
    }
    
    TTF_Quit();
    SDL_Quit();
}


//=============================================================
// 入力処理
//=============================================================

void Application::ProcessInput()
{
    mInputSys->PrepareForUpdate();
    
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_EVENT_QUIT:
                mIsActive = false;
                break;
            case SDL_EVENT_KEY_DOWN:
            {
                const SDL_KeyboardEvent& key = event.key;
                
                if ((key.scancode == SDL_SCANCODE_RETURN ||
                     key.scancode == SDL_SCANCODE_KP_ENTER) &&
                    (key.mod & SDL_KMOD_ALT) != 0)
                {
                    ToggleFullscreen();
                }
                break;
                
            }
            // -------------------------
            // ピクセルサイズ変更（HiDPI, モニタ移動など）
            // → Renderer に通知だけ
            // -------------------------
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                if (event.window.windowID == SDL_GetWindowID(mWindow))
                {
                    HandleWindowResized();
                }
                break;

            // -------------------------
            // 論理ウィンドウサイズ変更（ユーザーがリサイズした）
            // → アスペクト比を矯正
            // -------------------------
            case SDL_EVENT_WINDOW_RESIZED:
            {
                if (!mWindow) break;
                if (event.window.windowID != SDL_GetWindowID(mWindow)) break;
                if (!mLockAspect) break;
                if (mIsFullScreen) break; // 全画面時は OS 任せ

                int w = event.window.data1;
                int h = event.window.data2;
                if (w <= 0 || h <= 0) break;

                // 自分が SetWindowSize したときに来るイベントはスキップ
                if (mIsAdjustingSize)
                {
                    // 実際のピクセルサイズに合わせて Renderer を更新
                    HandleWindowResized();
                    mIsAdjustingSize = false;
                    break;
                }

                float aspect = static_cast<float>(w) / static_cast<float>(h);

                int newW = w;
                int newH = h;

                if (aspect > mTargetAspect)
                {
                    // 横長すぎ → 高さ優先で幅を調整
                    newH = h;
                    newW = static_cast<int>(newH * mTargetAspect + 0.5f);
                }
                else if (aspect < mTargetAspect)
                {
                    // 縦長すぎ → 幅優先で高さを調整
                    newW = w;
                    newH = static_cast<int>(newW / mTargetAspect + 0.5f);
                }

                // 既に目標アスペクトならそのまま反映
                if (newW == w && newH == h)
                {
                    HandleWindowResized();
                }
                else
                {
                    mIsAdjustingSize = true;
                    SDL_SetWindowSize(mWindow, newW, newH);
                    // 実際の Renderer 更新は、後で来る RESIZED/PIXEL_SIZE_CHANGED で HandleWindowResized() が呼ばれる
                }
                break;
            }
                
            default:

                break;
        }
    }
    
    mInputSys->Update();
    const InputState& state = mInputSys->GetState();
    
    if (state.Keyboard.GetKeyState(SDL_SCANCODE_ESCAPE) == EReleased)
    {
        mIsActive = false;
    }
    
    if (state.Keyboard.GetKeyState(SDL_SCANCODE_SPACE) == EHeld)
    {
        mIsPause = true;
    }
    else
    {
        mIsPause = false;
    }
    
    for (auto& actor : mActors)
    {
        actor->ProcessInput(state);
    }
}


//=============================================================
// Actor 管理
//=============================================================

void Application::AddActor(std::unique_ptr<Actor> actor)
{
    if (mIsUpdatingActors)
    {
        mPendingActors.emplace_back(std::move(actor));
    }
    else
    {
        mActors.emplace_back(std::move(actor));
    }
}

void Application::DestroyActor(Actor* actor)
{
    if (actor)
    {
        actor->SetState(Actor::EDead);
    }
}


//=============================================================
// データロード／解放
//=============================================================

void Application::UnloadData()
{
    mActors.clear();

    if (mRenderer)
    {
        mRenderer->UnloadData();
    }
    if (mAssetManager)
    {
        mAssetManager->UnloadData();
    }
}

void Application::LoadData()
{
    // 必要なら派生クラスで override
}


//=============================================================
// ゲームメインルーチン（1フレーム更新）
//=============================================================

void Application::UpdateFrame()
{
    // 固定フレームレート (60fps 相当)
    const Uint64 frameDurationNS = 16'000'000;  // 16ms
    Uint64 now = SDL_GetTicksNS();

    while ((now - mTicksCount) < frameDurationNS)
    {
        SDL_Delay(1);
        now = SDL_GetTicksNS();
    }

    float deltaTime = (now - mTicksCount) / 1'000'000'000.0f;
    if (deltaTime > 0.05f)
    {
        deltaTime = 0.05f;
    }

    mTicksCount = now;
    
    if (mIsPause)
        return;
    
    mTimeOfDaySys->Update(deltaTime);
    UpdateGame(deltaTime);
    mPhysWorld->Test();
    
    mIsUpdatingActors = true;
    for (auto& a : mActors)
    {
        a->Update(deltaTime);
    }
    mIsUpdatingActors = false;
    
    for (auto& p : mPendingActors)
    {
        p->ComputeWorldTransform();
        mActors.emplace_back(std::move(p));
    }
    mPendingActors.clear();
    
    mActors.erase(
        std::remove_if(
            mActors.begin(),
            mActors.end(),
            [](const std::unique_ptr<Actor>& actor)
            {
                return actor->GetState() == Actor::EDead;
            }
        ),
        mActors.end()
    );
    
    if (mSoundMixer)
    {
        Matrix4 inv = GetRenderer()->GetInvViewMatrix();
        mSoundMixer->Update(deltaTime, inv);
    }
}


//=============================================================
// アセットディレクトリの設定
//=============================================================

void Application::InitAssetManager(const std::string& path, float dpi)
{
    mAssetManager->SetAssetsPath(path);
    mAssetManager->SetWindowDisplayScale(dpi);
}


//=============================================================
// スクリーン操作関連
//=============================================================

void Application::HandleWindowResized()
{
    if (!mWindow) return;

    int pixelW = 0;
    int pixelH = 0;
    SDL_GetWindowSizeInPixels(mWindow, &pixelW, &pixelH);

    mScreenWidth  = pixelW;
    mScreenHeight = pixelH;

    if (mRenderer)
    {
        mRenderer->OnWindowResized(pixelW, pixelH);
    }
}

void Application::SetFullscreen(bool enable)
{
    if (!mWindow)
        return;

    if (mIsFullScreen == enable)
        return;

    if (enable)
    {
        // ★ フルスクリーンに入る前に「論理ウィンドウサイズ」を保存
        int w = 0, h = 0;
        SDL_GetWindowSize(mWindow, &w, &h); // ← ピクセルではない
        mWindowedWidth  = w;
        mWindowedHeight = h;
    }

    if (!SDL_SetWindowFullscreen(mWindow, enable))
    {
        std::cerr << "[Application] SDL_SetWindowFullscreen failed: "
                  << SDL_GetError() << std::endl;
        return;
    }

    mIsFullScreen = enable;

    // 実ピクセルサイズを取り直して Renderer へ通知
    HandleWindowResized();

    if (!enable)
    {
        // フルスクリーン解除時に元の論理サイズへ戻す
        if (mWindowedWidth > 0 && mWindowedHeight > 0)
        {
            SDL_SetWindowSize(mWindow, mWindowedWidth, mWindowedHeight);
            SDL_SetWindowPosition(
                mWindow,
                SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED
            );

            // サイズ変更イベントが飛んでくるはずだが、
            // 念のためここでも更新しておいてもよい
            HandleWindowResized();
        }
    }
}

void Application::ToggleFullscreen()
{
    SetFullscreen(!mIsFullScreen);
}

} // namespace toy
