#pragma once
#include <string>
#include <vector>
#include <type_traits>
#include <algorithm>
#include <cctype>      // std::isspace, std::tolower, std::toupper

namespace StringUtil
{
//==============================================================================
// 任意型 → std::string 変換
//------------------------------------------------------------------------------
// ・std::string            → そのまま返す
// ・std::string へ変換可  → 暗黙変換して返す
// ・それ以外（数値など） → std::to_string() を使う
//
// ※ログ出力や簡易フォーマット用の「なんでも文字列化ヘルパ」
//==============================================================================

template<typename T>
std::string ToString(const T& v)
{
    if constexpr (std::is_same_v<T, std::string>)
    {
        // すでに std::string ならコピーして返す
        return v;
    }
    else if constexpr (std::is_convertible_v<T, std::string>)
    {
        // std::string に暗黙変換できる型（例：自作クラス）に対応
        return v;
    }
    else
    {
        // それ以外はとりあえず std::to_string に任せる
        return std::to_string(v);
    }
}

//==============================================================================
// 軽量フォーマット関数
//  対応形式：
//   "<<", "{}", "{N}", "{:02}", "{N:02}"
//
// 例：
//   Format("FPS=<< Frame={:03}", 60, 4)      -> "FPS=60 Frame=004"
//   Format("{:02} {:02}", 3, 7)              -> "03 07"
//   Format("X={0:03}, Y={1:02}", 5, 9)       -> "X=005, Y=09"
//==============================================================================

template<typename... Args>
std::string Format(const std::string& fmt, Args&&... args)
{
    // 引数を string 化
    std::vector<std::string> values = {
        ToString(std::forward<Args>(args))...
    };

    std::string result = fmt;

    // 自動インデックス（{} / {:02} / << 用）
    size_t autoIndex = 0;

    // --------------------------------------------------
    // 1) "<<” 置換（順番）
    // --------------------------------------------------
    {
        size_t pos = 0;
        while ((pos = result.find("<<", pos)) != std::string::npos)
        {
            if (autoIndex >= values.size()) break;

            result.replace(pos, 2, values[autoIndex]);
            pos += values[autoIndex].size();
            ++autoIndex;
        }
    }

    // --------------------------------------------------
    // 2) "{}" 置換（順番）
    // --------------------------------------------------
    {
        size_t pos = 0;
        while ((pos = result.find("{}", pos)) != std::string::npos)
        {
            if (autoIndex >= values.size()) break;

            result.replace(pos, 2, values[autoIndex]);
            pos += values[autoIndex].size();
            ++autoIndex;
        }
    }

    // --------------------------------------------------
    // 3) "{N}" / "{:02}" / "{N:02}"
    // --------------------------------------------------
    {
        size_t pos = 0;

        while ((pos = result.find('{', pos)) != std::string::npos)
        {
            size_t end = result.find('}', pos);
            if (end == std::string::npos)
                break;

            std::string inside = result.substr(pos + 1, end - pos - 1);

            int index = -1;
            int width = -1;
            bool ok   = true;

            size_t colon = inside.find(':');

            // ------------------------------
            // index / width 解析
            // ------------------------------
            if (colon == std::string::npos)
            {
                // "{0}"
                if (!inside.empty() &&
                    std::all_of(inside.begin(), inside.end(), ::isdigit))
                {
                    index = std::stoi(inside);
                }
                else
                {
                    ok = false;
                }
            }
            else
            {
                // "{N:W}" or "{:W}"
                std::string sIndex = inside.substr(0, colon);
                std::string sWidth = inside.substr(colon + 1);

                if (!sWidth.empty() &&
                    std::all_of(sWidth.begin(), sWidth.end(), ::isdigit))
                {
                    width = std::stoi(sWidth);
                }
                else ok = false;

                if (sIndex.empty())
                {
                    // ★ 番号なし → 自動順番
                    index = static_cast<int>(autoIndex++);
                }
                else if (std::all_of(sIndex.begin(), sIndex.end(), ::isdigit))
                {
                    index = std::stoi(sIndex);
                }
                else ok = false;
            }

            // ------------------------------
            // 置換
            // ------------------------------
            if (ok && index >= 0 && index < (int)values.size())
            {
                std::string replacement = values[index];

                // ゼロ埋め
                if (width > 0 && (int)replacement.size() < width)
                {
                    replacement =
                        std::string(width - replacement.size(), '0') + replacement;
                }

                result.replace(pos, end - pos + 1, replacement);
                pos += replacement.size();
                continue;
            }

            // 置換できなければスキップ
            pos = end + 1;
        }
    }

    return result;
}

//==============================================================================
// Trim 系（前後の空白を削る）
//------------------------------------------------------------------------------
// ・LTrim : 先頭側だけ空白を削除
// ・RTrim : 末尾側だけ空白を削除
// ・Trim  : 両側を削除
//
// ※空白判定は std::isspace に丸投げ（スペース／タブ／改行など）
//==============================================================================

// 左側の空白除去
inline std::string LTrim(const std::string& s)
{
    size_t pos = 0;
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos])))
    {
        ++pos;
    }
    return s.substr(pos);
}

// 右側の空白除去
inline std::string RTrim(const std::string& s)
{
    if (s.empty()) return s;

    size_t end = s.size() - 1;
    while (end > 0 && std::isspace(static_cast<unsigned char>(s[end])))
    {
        --end;
    }

    return s.substr(0, end + 1);
}

// 両側の空白除去
inline std::string Trim(const std::string& s)
{
    return RTrim(LTrim(s));
}


//==============================================================================
// Split / Join
//------------------------------------------------------------------------------
// Split(s, ',')
//   → 区切り文字ごとに文字列を分解（空文字列もそのまま拾う）
//
// Join({ "A", "B", "C" }, ",")
//   → "A,B,C"
//==============================================================================

// 区切り文字 split（空文字も拾う）
inline std::vector<std::string> Split(const std::string& s, char sep)
{
    std::vector<std::string> out;
    std::string current;

    for (char c : s)
    {
        if (c == sep)
        {
            out.emplace_back(current);
            current.clear();
        }
        else
        {
            current.push_back(c);
        }
    }
    out.emplace_back(current);
    return out;
}

// join
inline std::string Join(const std::vector<std::string>& arr, const std::string& sep)
{
    if (arr.empty()) return "";

    std::string out = arr[0];
    for (size_t i = 1; i < arr.size(); i++)
    {
        out += sep;
        out += arr[i];
    }
    return out;
}


//==============================================================================
// StartsWith / EndsWith
//------------------------------------------------------------------------------
// プレフィックス／サフィックスの単純な判定。
// 大文字小文字は区別する（必要なら ToLower と組み合わせる）
//==============================================================================

inline bool StartsWith(const std::string& s, const std::string& prefix)
{
    return s.size() >= prefix.size()
        && std::equal(prefix.begin(), prefix.end(), s.begin());
}

inline bool EndsWith(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size()
        && std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
}


//==============================================================================
// ToLower / ToUpper
//------------------------------------------------------------------------------
// ASCII 前提で、文字列全体を小文字／大文字に変換。
// （日本語などマルチバイトは考慮していない軽量版）
//==============================================================================

inline std::string ToLower(const std::string& s)
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

inline std::string ToUpper(const std::string& s)
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}


//==============================================================================
// ReplaceAll（部分文字列の一括置換）
//------------------------------------------------------------------------------
// ReplaceAll("Assets\\Hero\\hero.fbx", "\\", "/")
//   → "Assets/Hero/hero.fbx"
//
// ・from が空文字列の場合は何もしない（無限ループ防止）
//==============================================================================

inline std::string ReplaceAll(std::string s, const std::string& from, const std::string& to)
{
    if (from.empty()) return s;

    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos)
    {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}


//==============================================================================
// PadLeft / PadRight（固定長パディング）
//------------------------------------------------------------------------------
// PadLeft("42", 5, '0')  → "00042"
// PadRight("abc", 5, '_')→ "abc__"
//
// ・既に width 以上なら、そのまま返す
//==============================================================================

inline std::string PadLeft(const std::string& s, size_t width, char fill = ' ')
{
    if (s.size() >= width) return s;
    return std::string(width - s.size(), fill) + s;
}

inline std::string PadRight(const std::string& s, size_t width, char fill = ' ')
{
    if (s.size() >= width) return s;
    return s + std::string(width - s.size(), fill);
}


//==============================================================================
// Path Utilities（パス系ユーティリティ）
//------------------------------------------------------------------------------
// ファイルパス文字列から、ディレクトリ／ファイル名／拡張子などを取得。
// パス区切りは '/' と '\\' の両方をサポート。
//==============================================================================

//------------------------------------------------------------------------------
// GetDirectory
//------------------------------------------------------------------------------
// "Assets/Character/hero.fbx"     → "Assets/Character/"
// "C:\\Data\\hero.fbx"            → "C:\\Data\\"
// "hero.fbx"                      → ""（ディレクトリ成分なし）
//
// ・末尾の '/' または '\\' までを含めて返す
// ・区切り文字が見つからない場合は空文字列
//------------------------------------------------------------------------------
inline std::string GetDirectory(const std::string& path)
{
    // '/' と '\\' のどちらでも OK にしておく（Windows/Mac/Linux 両対応）
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos)
    {
        return "";
    }
    return path.substr(0, pos + 1);
}

//------------------------------------------------------------------------------
// GetFileName
//------------------------------------------------------------------------------
// "Assets/Character/hero.fbx"     → "hero.fbx"
// "C:\\Data\\hero.fbx"            → "hero.fbx"
// "hero.fbx"                      → "hero.fbx"
// "Assets/Character/"             → ""（ファイル名がないケース）
//------------------------------------------------------------------------------
inline std::string GetFileName(const std::string& path)
{
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos)
    {
        return path; // 区切りがなければ全体がファイル名とみなす
    }
    if (pos + 1 >= path.size())
    {
        return ""; // 末尾がスラッシュで終わっている（フォルダパス）場合
    }
    return path.substr(pos + 1);
}

//------------------------------------------------------------------------------
// GetExtension
//------------------------------------------------------------------------------
// "Assets/Character/hero.fbx"     → "fbx"
// "C:\\Data\\image.png"           → "png"
// "noext"                         → ""（拡張子なし）
// "archive.tar.gz"                → "gz"（最後の '.' 以降を拡張子とみなす）
//------------------------------------------------------------------------------
inline std::string GetExtension(const std::string& path)
{
    std::string filename = GetFileName(path);

    // '.' が含まれていなければ拡張子なし
    size_t pos = filename.find_last_of('.');
    if (pos == std::string::npos || pos + 1 >= filename.size())
    {
        return "";
    }
    return filename.substr(pos + 1);
}

//------------------------------------------------------------------------------
// GetFileNameWithoutExtension
//------------------------------------------------------------------------------
// "Assets/Character/hero.fbx"     → "hero"
// "C:\\Data\\image.png"           → "image"
// "noext"                         → "noext"
// "archive.tar.gz"                → "archive.tar"（最後の '.' より前を返す）
//------------------------------------------------------------------------------
inline std::string GetFileNameWithoutExtension(const std::string& path)
{
    std::string filename = GetFileName(path);

    size_t pos = filename.find_last_of('.');
    if (pos == std::string::npos)
    {
        // '.' が無いときは、そのままファイル名を返す
        return filename;
    }
    return filename.substr(0, pos);
}

} // namespace StringUtil
