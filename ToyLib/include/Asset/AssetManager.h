#pragma once
#include <unordered_map>
#include <memory>
#include <string>

namespace toy {

//=====================================================================
// AssetManager
//
// ToyLib における **アセット一元管理クラス**
//  - テクスチャ、メッシュ、サウンド、音楽、フォント をキャッシュ管理
//  - 同じファイルを複数回読み込まない（メモリ効率）
//  - Embedded Texture（FBX/GLTF の埋め込み画像）にも対応
//
// ※ ゲーム開始時に Application がインスタンスを保持し、
//    各 Actor / Component は必要に応じて AssetManager 経由で読み込む。
//=====================================================================
class AssetManager
{
public:
    AssetManager();

    //=========================================================
    // メッシュ（FBX/GLTF/OBJなど）取得
    //  - キャッシュされたものがあれば再利用
    //  - isRightHanded : 右手座標系モデル対応
    //=========================================================
    std::shared_ptr<class Mesh> GetMesh(const std::string& fileName,
                                        bool isRightHanded = false);

    //=========================================================
    // テクスチャ取得（PNG, JPG, DDS など）
    //=========================================================
    std::shared_ptr<class Texture> GetTexture(const std::string& fileName);

    //=========================================================
    // Embedded Texture 取得（aiTexture や GLB 内の画像データ）
    // nameKey : 識別用キー（"_EMBED_0" など）
    //=========================================================
    std::shared_ptr<class Texture> GetEmbeddedTexture(const std::string& nameKey,
                                                      const uint8_t* data,
                                                      size_t dataSize);
    //=========================================================
    // 白 1x1 テクスチャ生成
    // 識別キー不要
    //=========================================================
    std::shared_ptr<class Texture> GetWhite1x1Texture();

    //=========================================================
    // 効果音（SE）
    //=========================================================
    std::shared_ptr<class SoundEffect> GetSoundEffect(const std::string& fileName);

    //=========================================================
    // BGM（Music）
    //=========================================================
    std::shared_ptr<class Music> GetMusic(const std::string& fileName);

    //=========================================================
    // フォント（TextFont）
    //=========================================================
    std::shared_ptr<class TextFont> GetFont(const std::string& fileName,
                                            int pointSize);

    // アセットフォルダの基準パス（GameApp 側で設定）
    std::string GetAssetsPath() const { return mAssetsPath; }
    void SetAssetsPath(const std::string& path) { mAssetsPath = path; }

    // DPI スケール（UI などで使用）
    void SetWindowDisplayScale(float scale) { mWindowDisplayScale = scale; }

    // 登録済みアセットをすべて破棄（シーン切り替え等）
    void UnloadData();

private:
    //===========================
    // キャッシュ管理マップ群
    //===========================
    std::unordered_map<std::string, std::shared_ptr<class Texture>>     mTextures;
    std::unordered_map<std::string, std::shared_ptr<class Mesh>>        mMeshes;
    std::unordered_map<std::string, std::shared_ptr<class SoundEffect>> mSoundEffects;
    std::unordered_map<std::string, std::shared_ptr<class Music>>       mMusics;
    std::unordered_map<std::string, std::shared_ptr<class TextFont>>    mTextFonts;

    // アセットの基準パス（GameApp 側で設定）
    std::string mAssetsPath{};

    // DPI スケール（UI 調整用）
    float mWindowDisplayScale{1.0f};
};

} // namespace toy
