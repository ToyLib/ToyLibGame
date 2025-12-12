// Graphics/Effect/GPUParticleComponent.h
#pragma once

#include "Graphics/VisualComponent.h"
#include "Utils/MathUtil.h"
#include <memory>
#include <string>

namespace toy {

class Texture;
class Shader;

//======================================================================
// GPUParticleComponent
//----------------------------------------------------------------------
// GPU（OpenGL）で更新・描画を行うパーティクルコンポーネント。
// ・更新：Transform Feedback / ping-pong VBO などを用いて GPU 側で進める
// ・描画：インスタンシング（quad + per-particle 属性）を想定
//
// 目的：CPU パーティクルよりも大量描画に強く、演出用エフェクトを軽くする。
// 設計：VisualComponent として VisualLayer に乗せて描画パスを統一する。
//======================================================================
class GPUParticleComponent : public VisualComponent
{
public:
    //==============================================================
    // ParticleMode
    //  代表的なエフェクト種別（プリセット用）
    //  ・Spark : 火花/キラキラ系
    //  ・Water : 水しぶき/雨/スプラッシュ系
    //  ・Smoke : 煙/霧/もや系
    //==============================================================
    enum class ParticleMode
    {
        Spark = 0,
        Water = 1,
        Smoke = 2
    };

    //==============================================================
    // Desc
    //  初期化用の設定データ。
    //  JSON などの外部データからも読み込みやすいように、
    //  1つの構造体にまとめている。
    //
    //  ※ 0 = infinite / disabled などの「意味のある 0」を採用している項目あり
    //==============================================================
    struct Desc
    {
        // モード（必要なら preset が適用される）
        ParticleMode mode = ParticleMode::Spark;

        // GPU バッファ上の最大パーティクル数（容量）
        uint32_t maxParticles = 64;

        // コンポーネント寿命（秒）
        // 0 = 無限（Stop() されるまで継続）
        float componentLife   = 0.0f;

        // 1 粒の寿命（秒）
        float particleLife    = 0.6f;

        // 1 粒のサイズ（ワールド or スクリーン基準は実装側の想定に従う）
        float size            = 1.0f;

        // 再スポーン頻度（1秒あたりの目安）
        // ※ GPU 更新なので「厳密に毎秒 N 個」ではなく近似/平均として扱う想定
        float spawnRatePerSec = 60.0f;

        // スポーン数を徐々に増やすランプ時間（秒）
        // 0 = ランプ無し（即座に spawnRatePerSec へ）
        float spawnRampSec    = 0.0f;

        // 放射の広がり（初速方向/位置のばらつき等に使用する想定）
        float spread          = 2.0f;

        // 重力（下方向の加速度）
        float gravity         = 0.0f;

        // 揚力（上方向の加速度：煙などで使用）
        float lift            = 0.0f;

        // ブレンド設定：true なら加算合成（発光系向け）
        bool  additiveBlend   = true;

        // ウォームスタート：初期化直後から「既に飛んでいる」状態にする
        // true ならバッファ初期値をランダム分布させ、見た目の立ち上がりを自然にする
        bool  warmStart       = true;

        // エミッタのオフセット（Actor ローカル空間）
        Vector3 emitterOffset = Vector3::Zero;
    };

public:
    //==============================================================
    // 生成・破棄
    //  drawOrder はエフェクトが「後段に重ねる」想定でやや大きめがデフォルト
    //==============================================================
    GPUParticleComponent(class Actor* owner, int drawOrder = 20);
    ~GPUParticleComponent();

    //==============================================================
    // VisualComponent interface
    //==============================================================
    void Update(float deltaTime) override;
    void Draw() override;

    // テクスチャ差し替え（パーティクル描画用）
    void SetTexture(std::shared_ptr<Texture> tex) override;

    //==============================================================
    // API
    //  Init()         : Desc を直接渡して初期化
    //  InitFromFile() : JSON 等のファイルから設定を読み込んで初期化
    //  Start/Stop     : 発生の開始/停止（Stop してもバッファは維持する想定）
    //  Reset()        : 時間・寿命・パーティクル状態をリセット（見た目も初期化）
    //==============================================================
    void Init(const Desc& desc);
    //==============================================================
    // InitFromFile
    //--------------------------------------------------------------
    // 外部ファイル（JSON）から Desc を読み込み、パーティクルを初期化する。
    //
    // 設計方針：
    // ・JSON に存在するキーのみ Desc に反映する
    // ・存在しない項目は Desc のデフォルト値をそのまま使用
    // ・Init() と同じ最終結果になることを保証する
    //
    // これにより：
    // ・JSON は「差分定義」で書ける
    // ・完全なサンプル JSON は別途用意可能
    // ・コード側の初期値が常に安全なフォールバックになる
    //
    // -------------------------------------------------------------
    // JSON フォーマット例：
    //
    // {
    //   "mode": "Spark",
    //   "maxParticles": 128,
    //   "particleLife": 0.8,
    //   "size": 1.2,
    //
    //   "spawnRatePerSec": 80.0,
    //   "spawnRampSec": 0.5,
    //
    //   "spread": 3.0,
    //   "gravity": -9.8,
    //   "lift": 2.0,
    //
    //   "additiveBlend": true,
    //   "warmStart": true,
    //
    //   "emitterOffset": [0.0, 1.0, 0.0]
    // }
    //
    // ・mode は文字列（Spark / Water / Smoke）で指定
    // ・emitterOffset は [x, y, z] の配列
    // ・componentLife を省略すると「無限」
    //
    // 戻り値：
    // ・true  : 読み込み＆初期化成功
    // ・false : ファイル不正 or 読み込み失敗
    //==============================================================
    bool InitFromFile(const std::string& filePath);
    void Start();
    void Stop();
    void Reset();

private:
    //==============================================================
    // 内部ユーティリティ
    //==============================================================
    static ParticleMode ParseModeString(const std::string& s);

    // mode に応じた preset を適用（Desc が未指定/デフォルトの場合のみ等）
    void ApplyModePresetIfNeeded();

    // まだ GL リソースが未生成なら初期化（遅延初期化）
    void InitIfNeeded();

    // GL リソース破棄（VBO/VAO 等）
    void ReleaseGL();

    // quad（板ポリ）形状生成（描画用）
    void InitQuadGeometry();

    // 更新用 VAO（Transform Feedback 等の入力属性を束縛）
    void InitUpdateVAO();

    // 描画用 VAO（インスタンシング属性を束縛）
    void InitRenderVAO();

    // パーティクルバッファ生成（ping-pong 用 A/B）
    void InitParticleBuffers(bool warmStart);

    // GPU 側で粒の更新（dt を渡す）
    void UpdateParticlesGPU(float dt);

    // ping-pong の現在の読み元/書き先 VBO を取得
    unsigned int CurrentSrcVBO() const;
    unsigned int CurrentDstVBO() const;

    // Update 用 attribute のバインド
    void BindUpdateAttributes(unsigned int srcVBO);

    // Render 用 instance attribute のバインド
    void BindInstanceAttributes(unsigned int srcVBO);

private:
    //==============================================================
    // Assets（描画/更新に必要なリソース）
    //==============================================================
    std::shared_ptr<Texture> mTexture;
    std::shared_ptr<Shader>  mUpdateShader; // 粒更新用
    std::shared_ptr<Shader>  mRenderShader; // 粒描画用

    //==============================================================
    // 状態
    //==============================================================
    Desc  mDesc{};
    bool  mInitialized;         // GL リソース生成済みか
    bool  mPingPong;            // 現在どちらの VBO を src にしているか
    bool  mRunning;             // 発生/更新が有効か

    // 時間管理
    float mComponentLifeAcc;    // コンポーネント寿命の経過
    float mTimeAcc;             // スポーン制御などに使う時間

    //==============================================================
    // GL リソース
    //==============================================================
    unsigned int mQuadVBO;
    unsigned int mQuadIBO;

    // パーティクルデータ（ping-pong）
    unsigned int mParticleVBO_A;
    unsigned int mParticleVBO_B;

    // VAO
    unsigned int mUpdateVAO;
    unsigned int mRenderVAO;
};

} // namespace toy

