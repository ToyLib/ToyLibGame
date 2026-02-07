#include "Render/GL/Shader.h"

namespace toy {

// 同値スキップ用
GLuint Shader::sCurrentShaderID = 0;

//=============================================================
// シェーダープログラム読み込み／セットアップ
//=============================================================

// シェーダー読み込み
//  - 頂点シェーダー／フラグメントシェーダーをコンパイルしてリンクする
//  - 成功すると mShaderProgramID が有効なプログラムになる
bool Shader::Load(const std::string& vertName, const std::string& fragName)
{
    // 頂点シェーダーコンパイル
    if (!CompileShader(vertName, GL_VERTEX_SHADER, mVertexShaderID))
    {
        return false;
    }
    
    // フラグメントシェーダーコンパイル
    if (!CompileShader(fragName, GL_FRAGMENT_SHADER, mFragShaderID))
    {
        return false;
    }
    
    // シェーダープログラム作成＆リンク
    mShaderProgramID = glCreateProgram();
    glAttachShader(mShaderProgramID, mVertexShaderID);
    glAttachShader(mShaderProgramID, mFragShaderID);
    glLinkProgram(mShaderProgramID);
    
    // リンクエラーがないかチェック
    if (!IsValidProgram())
    {
        return false;
    }
    
    return true;
}

bool Shader::LoadWithTransformFeedback(
    const std::string& vertName,
    const std::string& fragName,
    const std::vector<const char*>& varyings,
    GLenum bufferMode)
{
    // vertex
    if (!CompileShader(vertName, GL_VERTEX_SHADER, mVertexShaderID))
    {
        return false;
    }
    // fragment は optional（Update用はVSだけでOK）
    bool hasFrag = !fragName.empty();
    if (hasFrag)
    {
        if (!CompileShader(fragName, GL_FRAGMENT_SHADER, mFragShaderID))
        {
            return false;
        }
    }
    else
    {
        mFragShaderID = 0;
    }

    mShaderProgramID = glCreateProgram();
    glAttachShader(mShaderProgramID, mVertexShaderID);
    if (hasFrag)
    {
        glAttachShader(mShaderProgramID, mFragShaderID);
    }
    
    // ★ここがTFの肝：link前に varyings 指定
    if (!varyings.empty())
    {
        glTransformFeedbackVaryings(
            mShaderProgramID,
            (GLsizei)varyings.size(),
            varyings.data(),
            bufferMode
        );
    }

    glLinkProgram(mShaderProgramID);

    if (!IsValidProgram())
    {
        return false;
    }
    
    return true;
}

// GL リソース解放
void Shader::Unload()
{
    glDeleteProgram(mShaderProgramID);
    glDeleteShader(mVertexShaderID);
    glDeleteShader(mFragShaderID);
}

// このシェーダープログラムを OpenGL にバインド
void Shader::SetActive()
{
    if (sCurrentShaderID == mShaderProgramID)
    {
        return;
    }
    glUseProgram(mShaderProgramID);
    sCurrentShaderID = mShaderProgramID;
}


//=============================================================
// Uniform セット系
//=============================================================

// 4x4 行列を uniform に送る
void Shader::SetMatrixUniform(const char* name, const Matrix4& matrix)
{
    GLint loc = glGetUniformLocation(mShaderProgramID, name);
    glUniformMatrix4fv(loc, 1, GL_TRUE, matrix.GetAsFloatPtr());
}

// 4x4 行列配列を uniform に送る（スキンメッシュのボーン行列など）
void Shader::SetMatrixUniforms(const char* name, const Matrix4* matrices, unsigned count)
{
    GLint loc = glGetUniformLocation(mShaderProgramID, name);
    glUniformMatrix4fv(loc, count, GL_TRUE, matrices[0].GetAsFloatPtr());
}

// vec3 を uniform に送る
void Shader::SetVectorUniform(const char* name, const Vector3& vector)
{
    GLint loc = glGetUniformLocation(mShaderProgramID, name);
    glUniform3fv(loc, 1, vector.GetAsFloatPtr());
}

// vec2 を uniform に送る
void Shader::SetVector2Uniform(const char* name, const Vector2& vector)
{
    GLint loc = glGetUniformLocation(mShaderProgramID, name);
    glUniform2fv(loc, 1, vector.GetAsFloatPtr());
}

// float を uniform に送る
void Shader::SetFloatUniform(const char* name, float value)
{
    GLint loc = glGetUniformLocation(mShaderProgramID, name);
    glUniform1f(loc, value);
}

// bool を uniform に送る（内部的には int として送る）
void Shader::SetBooleanUniform(const char *name, bool value)
{
    GLint loc = glGetUniformLocation(mShaderProgramID, name);
    glUniform1i(loc, value);
}

// sampler 用のテクスチャユニット番号を送る
void Shader::SetTextureUniform(const char* name, GLuint textureUnit)
{
    GLint loc = glGetUniformLocation(mShaderProgramID, name);
    glUniform1i(loc, textureUnit);
}

// int を uniform に送る
void Shader::SetIntUniform(const char* name, int value)
{
    GLint loc = glGetUniformLocation(mShaderProgramID, name);
    glUniform1i(loc, value);
}


//=============================================================
// シェーダーコンパイル／リンクエラー確認
//=============================================================

// シェーダーファイルを読み込んでコンパイル
//  - fileName  : GLSL ファイルパス
//  - shaderType: GL_VERTEX_SHADER / GL_FRAGMENT_SHADER など
//  - outShader : コンパイル済みシェーダー ID を返す
bool Shader::CompileShader(const std::string& fileName, GLenum shaderType, GLuint& outShader)
{
    std::ifstream shaderFile(fileName);
    if (shaderFile.is_open())
    {
        // ソース全体読み込み
        std::stringstream sstream;
        sstream << shaderFile.rdbuf();
        std::string contents = sstream.str();
        const char* contentsChar = contents.c_str();
        
        // シェーダー作成＆コンパイル
        outShader = glCreateShader(shaderType);
        glShaderSource(outShader, 1, &(contentsChar), nullptr);
        glCompileShader(outShader);
        
        // コンパイル結果チェック
        if (!IsCompiled(outShader))
        {
            std::cerr << "Failed to compile shader: "
                      << fileName.c_str() << std::endl;
            return false;
        }
    }
    else
    {
        std::cerr << "Shader file not found: "
                  << fileName.c_str() << std::endl;
        return false;
    }
    
    return true;
}

// シェーダーコンパイル結果チェック
bool Shader::IsCompiled(GLuint shader)
{
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    
    if (status != GL_TRUE)
    {
        char buffer[512];
        memset(buffer, 0, 512);
        glGetShaderInfoLog(shader, 511, nullptr, buffer);
        std::cerr << "GLSL Compile Failed: "
                  << buffer << std::endl;
        return false;
    }
    
    return true;
}

// プログラムリンク結果チェック
bool Shader::IsValidProgram()
{
    GLint status;
    glGetProgramiv(mShaderProgramID, GL_LINK_STATUS, &status);
    if (status != GL_TRUE)
    {
        char buffer[512];
        memset(buffer, 0, 512);
        glGetProgramInfoLog(mShaderProgramID, 511, nullptr, buffer);
        std::cerr << "GLSL Link Failed: "
                  << buffer << std::endl;
        return false;
    }
    
    return true;
}

} // namespace toy
