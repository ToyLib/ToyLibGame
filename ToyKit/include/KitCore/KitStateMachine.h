#pragma once

#include <cassert>
#include <functional>
#include <unordered_map>

namespace toy::kit {

//=============================================================================
// KitStateMachine<TStateEnum>
//
//  ゲームオブジェクトの行動を「ステート」単位で管理する汎用ステートマシン。
//  TStateEnum には enum class（または int）を渡す。
//
//  【基本的な使い方】
//
//    enum class WolfState { Idle, Walk, Run };
//
//    KitStateMachine<WolfState> mFSM;
//
//    // コンストラクタなどで全ステートを登録する
//    mFSM.Register(WolfState::Idle,
//        /* onUpdate */ [&](float dt)
//        {
//            if (mFSM.IsEnterFrame()) animPlayer->Play(ANIM_IDLE);
//            if (mFSM.GetTimer() > 3.0f) mFSM.To(WolfState::Walk);
//        },
//        /* onEnter */ [&]{ /* 入場時に1度だけ実行 */ },
//        /* onExit  */ [&]{ /* 退場時に1度だけ実行 */ }
//    );
//    // ... 他のステートも Register ...
//
//    mFSM.Start(WolfState::Idle);  // 初期ステートを指定して開始
//
//    // UpdateActor で毎フレーム呼ぶだけ
//    void WolfActor::UpdateActor(float dt) { mFSM.Update(dt); }
//
//  【遷移ルール】
//    - To() を呼んだ次の Update() 先頭で遷移が確定する
//    - 同フレームに複数 To() を呼んだ場合は「最後の値」が有効
//    - 現在と同じステートへの To() は無視される（enter/exit 不発）
//    - onUpdate の中から To() を呼んでよい
//
//  【IsEnterFrame について】
//    ステートに入った直後（最初の Update 呼び出し）の間だけ true になる。
//    アニメ切替など「1度だけやりたい」初期化に使う。
//    onEnter コールバックと違い、onUpdate の中から参照できる点が便利。
//=============================================================================

template<typename TStateEnum>
class KitStateMachine
{
public:
    using UpdateFn   = std::function<void(float)>;  // onUpdate(deltaTime)
    using CallbackFn = std::function<void()>;        // onEnter / onExit

    //-------------------------------------------------------------------------
    // ステートを登録する
    //  state    : ステート識別子（enum の値）
    //  onUpdate : 毎フレーム呼ばれる処理（必須）
    //  onEnter  : このステートに入った瞬間（省略可）
    //  onExit   : このステートから出る瞬間（省略可）
    //
    //  ※ Register は Start() より前に呼ぶこと
    //-------------------------------------------------------------------------
    void Register(TStateEnum state,
                  UpdateFn   onUpdate,
                  CallbackFn onEnter = {},
                  CallbackFn onExit  = {});

    //-------------------------------------------------------------------------
    // 初期ステートを指定して開始する
    //  - 指定ステートの onEnter を呼ぶ
    //  - Register 後、最初の Update() 前に呼ぶこと
    //  - 2 回呼ぶと assert で止まる
    //-------------------------------------------------------------------------
    void Start(TStateEnum initialState);

    //-------------------------------------------------------------------------
    // ステート遷移を予約する
    //  - 次の Update() 先頭で適用される（onUpdate 完了後ではない）
    //  - onUpdate の内部から安全に呼べる
    //-------------------------------------------------------------------------
    void To(TStateEnum nextState);

    //-------------------------------------------------------------------------
    // 毎フレーム呼ぶ（UpdateActor の deltaTime をそのまま渡す）
    //-------------------------------------------------------------------------
    void Update(float deltaTime);

    //-------------------------------------------------------------------------
    // 情報取得
    //-------------------------------------------------------------------------

    // 現在のステート
    TStateEnum GetState() const { return mCurrent; }

    // 現在のステートに入ってからの経過時間（秒）
    //  - ステートが切り替わるとリセットされる
    float GetTimer() const { return mTimer; }

    // 現在の Update() が、このステートに入った最初のフレームかどうか
    //  - true の間に animPlayer->Play() を呼ぶとアニメが一度だけ切り替わる
    bool IsEnterFrame() const { return mIsEnterFrame; }

private:
    //--- 1 ステート分のデータ -----------------------------------------------
    struct StateEntry
    {
        UpdateFn   onUpdate;
        CallbackFn onEnter;
        CallbackFn onExit;
    };

    // enum / int どちらでも key にできる単純なハッシャー
    struct StateHash
    {
        std::size_t operator()(TStateEnum e) const noexcept
        {
            return static_cast<std::size_t>(e);
        }
    };

    using StateMap = std::unordered_map<TStateEnum, StateEntry, StateHash>;

    //--- フィールド -----------------------------------------------------------
    StateMap   mStates;
    TStateEnum mCurrent      {};   // 現在のステート
    TStateEnum mPending      {};   // 遷移先（予約中）
    bool       mHasPending   = false;
    bool       mStarted      = false;
    float      mTimer        = 0.0f;
    bool       mIsEnterFrame = false;

    //--- 内部遷移処理 ---------------------------------------------------------
    void ApplyTransition(TStateEnum next);
};

//=============================================================================
// 実装（header-only template）
//=============================================================================

template<typename T>
void KitStateMachine<T>::Register(T state,
                                   UpdateFn   onUpdate,
                                   CallbackFn onEnter,
                                   CallbackFn onExit)
{
    mStates[state] = { std::move(onUpdate), std::move(onEnter), std::move(onExit) };
}

template<typename T>
void KitStateMachine<T>::Start(T initialState)
{
    assert(!mStarted && "KitStateMachine::Start() が 2 回呼ばれました");

    mStarted      = true;
    mCurrent      = initialState;
    mTimer        = 0.0f;
    mIsEnterFrame = true;

    if (auto it = mStates.find(initialState); it != mStates.end())
        if (it->second.onEnter) it->second.onEnter();
}

template<typename T>
void KitStateMachine<T>::To(T nextState)
{
    mPending    = nextState;
    mHasPending = true;
}

template<typename T>
void KitStateMachine<T>::Update(float deltaTime)
{
    assert(mStarted && "KitStateMachine::Update() の前に Start() を呼んでください");

    // 前フレームの To() で予約された遷移を先頭で適用する
    if (mHasPending)
    {
        mHasPending = false;
        if (mPending != mCurrent)
            ApplyTransition(mPending);
    }

    // タイマーを進める（遷移直後は 0 からカウント）
    mTimer += deltaTime;

    // 現在ステートの更新処理を実行
    // ※ IsEnterFrame() はこの onUpdate が終わるまで true のまま
    if (auto it = mStates.find(mCurrent); it != mStates.end())
        if (it->second.onUpdate) it->second.onUpdate(deltaTime);

    // onUpdate が終わってからフラグをリセット
    mIsEnterFrame = false;
}

template<typename T>
void KitStateMachine<T>::ApplyTransition(T next)
{
    // 現在ステートの退場処理
    if (auto it = mStates.find(mCurrent); it != mStates.end())
        if (it->second.onExit) it->second.onExit();

    mCurrent      = next;
    mTimer        = 0.0f;
    mIsEnterFrame = true;

    // 次ステートの入場処理
    if (auto it = mStates.find(next); it != mStates.end())
        if (it->second.onEnter) it->second.onEnter();
}

} // namespace toy::kit
