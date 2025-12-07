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
    // オブジェクト型のサブ要素取得
    //--------------------------------------------------------------------------

    // obj[key] が object の場合、そのまま out に代入して true。
    // それ以外（存在しない or object でない）は false。
    bool GetObject(const nlohmann::json& obj, const char* key, nlohmann::json& out);
} // namespace JsonHelper
