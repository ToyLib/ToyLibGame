#pragma once

#include <string>
#include <SDL3_ttf/SDL_ttf.h>

namespace toy {

//==============================================================
// TextFont
//   - SDL3_ttf の TTF_Font をラップする軽量クラス
//   - AssetManager により shared_ptr<TextFont> 管理される前提
//==============================================================
class TextFont
{
public:
    TextFont();
    ~TextFont();
    
    //------------------------------------------------------------------
    // Load
    //   - TTF_OpenFont() を使いフォントをメモリにロード
    //   - 同じ TextFont インスタンスで何度も Load() した場合は
    //     前のフォントを自動 Unload() してからロードし直す
    //------------------------------------------------------------------
    bool Load(const std::string& filePath, int pointSize);
    
    //------------------------------------------------------------------
    // Unload
    //   - TTF_CloseFont(mFont)
    //   - 呼ばなくても破棄時に自動で開放される
    //------------------------------------------------------------------
    void Unload();
    
    //------------------------------------------------------------------
    // IsValid
    //   - フォントが正常にロードできたか確認
    //------------------------------------------------------------------
    bool IsValid() const { return mFont != nullptr; }
    
    //------------------------------------------------------------------
    // SDL_ttf の生フォントポインタ
    //   - Renderer::CreateTextTexture() などが利用
    //------------------------------------------------------------------
    TTF_Font* GetNativeFont() const { return mFont; }
    
    //------------------------------------------------------------------
    // 情報取得
    //------------------------------------------------------------------
    const std::string& GetFilePath() const { return mFilePath; }
    int GetPointSize() const { return mPointSize; }
    
private:
    // コピー禁止（AssetManager 経由の shared_ptr 前提）
    TextFont(const TextFont&) = delete;
    TextFont& operator=(const TextFont&) = delete;
    
    TTF_Font*    mFont      {nullptr};
    std::string  mFilePath  {""};
    int          mPointSize {};
};

} // namespace toy
