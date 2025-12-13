#include "Engine/Runtime/SingleInstance.h"
#include <iostream>

#ifdef _WIN32
    // Windows では CreateMutex を使うので追加のヘッダは不要
#else
    #include <fcntl.h>
    #include <unistd.h>
    #include <errno.h>
    #include <cstring>
#endif

//==============================
// ロックファイル名（UNIX系）
//   - /tmp 配下にロック用ファイルを作成
//   - 同時起動チェックに使用
//==============================
const char* LOCK_FILE_NAME = "/tmp/my_program.lock";

namespace toy {

//======================================================================
// SingleInstance
//   - アプリの多重起動を防ぐためのユーティリティ
//   - コンストラクタでロック取得を試み、取得できなければ IsLocked() が false
//   - デストラクタでロック解放（Windows: Mutex / UNIX: lockf + ファイル削除）
//======================================================================

SingleInstance::SingleInstance()
    : mIsLocked(false)
{
#ifdef _WIN32
    // グローバル名前空間の Mutex を作成（すでに存在する場合は多重起動）
    mMutex = CreateMutexA(NULL, FALSE, "Global:ToyLib_Sample_SingleInstance_Mutex");
    if (mMutex == NULL)
    {
        std::cerr << "CreateMutex failed: " << GetLastError() << std::endl;
        return;
    }

    // すでに同名の Mutex が存在していた場合は多重起動とみなす
    DWORD err = GetLastError();
    if (err == ERROR_ALREADY_EXISTS)
    {
        std::cerr << "プログラムはすでに起動しています。GetLastError: "
                  << err << std::endl;
        CloseHandle(mMutex);
        mMutex = NULL;
        return;
    }
#else
    // /tmp にロック用ファイルを作成・オープン
    mFd = open(LOCK_FILE_NAME, O_CREAT | O_RDWR, 0666);
    if (mFd == -1)
    {
        // ファイル自体が開けない場合
        perror("ロックファイルを開けませんでした");
        return;
    }
    
    // lockf による排他ロックを試みる
    //   - すでにロックされていれば失敗＝他インスタンス起動中
    if (lockf(mFd, F_TLOCK, 0) == -1)
    {
        std::cerr << "プログラムはすでに起動しています: "
                  << strerror(errno) << std::endl;
        close(mFd);
        return;
    }
#endif

    // ここまで来たらロック取得成功
    mIsLocked = true;
}

SingleInstance::~SingleInstance()
{
    if (mIsLocked)
    {
#ifdef _WIN32
        // Mutex を閉じてロック解放
        if (mMutex)
        {
            CloseHandle(mMutex);
        }
#else
        // ファイルディスクリプタを閉じ、ロックファイルを削除
        if (mFd != -1)
        {
            close(mFd);
            unlink(LOCK_FILE_NAME);
        }
#endif
    }
}

} // namespace toy
