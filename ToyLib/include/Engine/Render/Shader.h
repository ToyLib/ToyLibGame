#pragma once

#include "Utils/MathUtil.h"
#include "glad/glad.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

namespace toy {

//-------------------------------------------------------------
// Shader
// ・頂点シェーダ／フラグメントシェーダを読み込み＆リンクして
//   OpenGL のシェーダプログラムとして管理するクラス。
// ・SetActive() で glUseProgram() 相当を行い、各種 uniform を設定。
//-------------------------------------------------------------
class Shader
{
public:
    Shader();
    ~Shader();
    
    //---------------------------------------------------------
    // シェーダ読み込み／破棄
    //---------------------------------------------------------
    
    // シェーダプログラムを読み込み＆コンパイル＆リンク
    //   vertName: 頂点シェーダのファイル名
    //   fragName: フラグメントシェーダのファイル名
    bool Load(const std::string& vertName, const std::string& fragName);
    
    // シェーダプログラムと個別シェーダを破棄
    void Unload();
    
    // このシェーダをアクティブにする（glUseProgram）
    void SetActive();
    
    
    //---------------------------------------------------------
    // uniform 設定（行列・ベクトル・スカラー等）
    //---------------------------------------------------------
    
    // 4x4 行列
    void SetMatrixUniform(const char* name, const Matrix4& matrix);
    
    // 行列配列（スキニング等で使用）
    void SetMatrixUniforms(const char* name, Matrix4* matrices, unsigned count);
    
    // 3D ベクトル
    void SetVectorUniform(const char* name, const Vector3& vector);
    
    // 2D ベクトル
    void SetVector2Uniform(const char* name, const Vector2& vector);
    
    // float
    void SetFloatUniform(const char* name, float value);
    
    // bool（内部では int で渡すことが多い）
    void SetBooleanUniform(const char* name, bool value);
    
    // テクスチャユニット番号（sampler2D 等と対応）
    void SetTextureUniform(const char* name, GLuint textureUnit);
    
    // int
    void SetIntUniform(const char* name, int value);
    
    
private:
    //---------------------------------------------------------
    // OpenGL オブジェクト ID
    //---------------------------------------------------------
    
    GLuint mVertexShaderID;    // 頂点シェーダ
    GLuint mFragShaderID;      // フラグメントシェーダ
    GLuint mShaderProgramID;   // リンク済みプログラム
    
    
    //---------------------------------------------------------
    // 内部ヘルパー関数
    //---------------------------------------------------------
    
    // シェーダファイルを読み込み、コンパイル
    bool CompileShader(const std::string& fileName, GLenum shaderType, GLuint& outShader);
    
    // シェーダコンパイル結果チェック
    bool IsCompiled(GLuint shader);
    
    // シェーダプログラムのリンク＆バリデーションチェック
    bool IsValidProgram();
};

} // namespace toy
