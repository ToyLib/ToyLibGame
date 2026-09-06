#pragma once

#include "json.hpp" // nlohmann::json の単一ヘッダ
#include <string>
#include <vector>
#include "Utils/MathUtil.h"

namespace JsonHelper
{
    //==========================================================================
    // 基本型の取得ヘルパー
    //--------------------------------------------------------------------------
    // ・key が存在し、かつ期待する型であれば out に代入して true を返す。
    // ・存在しない or 型が違う場合は何もせず false を返す。
    //==========================================================================

    bool GetInt   (const nlohmann::json& obj, const char* key, int&    out);
    bool GetFloat (const nlohmann::json& obj, const char* key, float&  out);
    bool GetBool  (const nlohmann::json& obj, const char* key, bool&   out);
    bool GetString(const nlohmann::json& obj, const char* key, std::string& out);

    //--------------------------------------------------------------------------
    // 文字列配列
    //--------------------------------------------------------------------------
    // ・key に対応する値が配列で、その中に string が含まれていれば out に詰める。
    // ・配列要素のうち string 以外はスキップ。
    // ・有効な string が 1 つも無い場合は false。
    //--------------------------------------------------------------------------

    bool GetStringArray(const nlohmann::json& obj,
                        const char* key,
                        std::vector<std::string>& out);

    //--------------------------------------------------------------------------
    // 数学型（Vector / Quaternion）
    //--------------------------------------------------------------------------

    // key: [x, y]
    bool GetVector2(const nlohmann::json& obj, const char* key, Vector2& out);

    // key: [x, y, z]
    bool GetVector3(const nlohmann::json& obj, const char* key, Vector3& out);

    // key: [pitch, yaw, roll]（単位：度）
    //  - X: pitch / Y: yaw / Z: roll を想定
    //  - ラジアンに変換してから Quaternion を構築
    bool GetQuaternionFromEuler(const nlohmann::json& obj,
                                const char* key,
                                Quaternion& out);

    //--------------------------------------------------------------------------
    // JSONファイル読み込み
    //--------------------------------------------------------------------------

    // 指定パスから JSON ファイルを開き、パース結果を out に格納する。
    // 失敗時は false（ファイルオープン失敗 or パース例外など）。
    bool LoadFromFile(const std::string& path, nlohmann::json& out);

    //--------------------------------------------------------------------------
    // デフォルト設定 + 実行環境ごとの上書き設定の読み込み
    //--------------------------------------------------------------------------
    // ・defaultPath を読み込んでベースとする（読み込めなければ false）。
    // ・userPath が存在すれば、その内容をベースにマージ（同じキーは上書き）。
    // ・userPath が存在しなければ、デフォルト値をそのまま userPath に新規生成する。
    bool LoadWithUserOverride(const std::string& defaultPath,
                              const std::string& userPath,
                              nlohmann::json& out);

    //--------------------------------------------------------------------------
    // オブジェクト型のサブ要素取得
    //--------------------------------------------------------------------------

    // obj[key] が object の場合、そのまま out に代入して true。
    // それ以外（存在しない or object でない）は false。
    bool GetObject(const nlohmann::json& obj, const char* key, nlohmann::json& out);

    //==========================================================================
    // 基本型の書き込みヘルパー
    //--------------------------------------------------------------------------
    // ・obj[key] に value を設定する（Get系と対称。書き込みは失敗しないので戻り値なし）。
    //==========================================================================

    void SetInt   (nlohmann::json& obj, const char* key, int value);
    void SetFloat (nlohmann::json& obj, const char* key, float value);
    void SetBool  (nlohmann::json& obj, const char* key, bool value);
    void SetString(nlohmann::json& obj, const char* key, const std::string& value);

    //--------------------------------------------------------------------------
    // 文字列配列の書き込み
    //--------------------------------------------------------------------------

    void SetStringArray(nlohmann::json& obj,
                        const char* key,
                        const std::vector<std::string>& value);

    //--------------------------------------------------------------------------
    // 数学型（Vector）の書き込み
    //--------------------------------------------------------------------------

    // key: [x, y]
    void SetVector2(nlohmann::json& obj, const char* key, const Vector2& value);

    // key: [x, y, z]
    void SetVector3(nlohmann::json& obj, const char* key, const Vector3& value);

    //--------------------------------------------------------------------------
    // オブジェクト型のサブ要素書き込み
    //--------------------------------------------------------------------------

    // obj[key] = value（サブオブジェクトをそのまま設定）
    void SetObject(nlohmann::json& obj, const char* key, const nlohmann::json& value);

    //--------------------------------------------------------------------------
    // JSONファイル書き込み
    //--------------------------------------------------------------------------

    // data を整形済み(indentスペース)JSONとして path に書き出す。
    // 失敗時は false（ファイルオープン失敗など）。
    bool SaveToFile(const std::string& path, const nlohmann::json& data, int indent = 4);
} // namespace JsonHelper
