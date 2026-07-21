<p align="center">
  <img src="./toylib_logo.png" alt="ToyLib logo" width="300"/>
</p>

# ToyLib

## A lightweight C++ game engine for learning modern game programming


<p align="center">
  <a href="./LICENSE">
    <img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="License: MIT">
  </a>
  <a href="https://github.com/ToyLib/ToyLibGame/issues">
    <img src="https://img.shields.io/github/issues/ToyLib/ToyLibGame" alt="Issues">
  </a>
  <a href="https://github.com/ToyLib/ToyLibGame/stargazers">
    <img src="https://img.shields.io/github/stars/ToyLib/ToyLibGame?style=social" alt="Stars">
  </a>
</p>

---

## 🧸 What is ToyLib?

ToyLibは、***Modern C++でゲームプログラミングを学ぶための軽量ゲームエンジン／クラスライブラリ***です。

  

「***ゲームプログラマのオモチャ***」をコンセプトに、ソースコード全体を理解できる規模を維持しながら、本格的な3Dゲームを開発できます。

  

巨大なゲームエンジンをブラックボックスとして利用するのではなく、自ら改造し、仕組みを理解しながらゲーム開発を楽しむことを目指しています。


---

# ライブラリ構成

  

## ToyLib（基本ライブラリ）

  

ゲームエンジンとして共通に利用する機能を提供します。

  

### Graphics

  

-   スケルタルアニメーション対応3Dモデル

-   GLTF（推奨）/ FBX / DirectX X対応

-   シャドウマッピング

-   パーティクル

-   ポイントライト

-   シーンキャプチャ

-   天候表現

-   昼夜サイクル

-   時間帯によるライティング変化

  

### Audio

  

-   3Dサウンド

-   BGMストリーミング再生

  

### Physics

  

-   衝突判定

-   重力処理

-   地形衝突

-   壁判定

-   距離・方角センサー

  

### Utility

  

-   JSONによる各種設定

    -   レンダラー

    -   アプリケーション

    -   パーティクル

-   デバッグ描画

-   ログ出力

  

------------------------------------------------------------------------

  

## ToyKit（ゲームテンプレート）

  

ゲーム開発を素早く始めるためのテンプレートライブラリです。

  

-   シーン管理・シーン遷移

-   ゲーム向けActorテンプレート

-   ゲーム向けComponentテンプレート

-   サンプルゲーム

  

------------------------------------------------------------------------

  

# クロスプラットフォーム

  

ToyLibは、同じゲームコードを

  

-   Windows

-   macOS

-   Linux

  

でそのままビルド・実行できる****完全なソース互換****を目標としています。

  

プラットフォーム固有のコードを書くことなく、各OSでゲームを開発できます。

  

------------------------------------------------------------------------

  

# 使用ライブラリ

  

-   SDL3

-   SDL3_image

-   SDL3_ttf

-   OpenGL

-   Vulkan

-   Assimp

-   OpenAL Soft

  

------------------------------------------------------------------------

  

# 開発環境

  

## Windows

  

-   Visual Studio 2026

-   Visual Studio Code

-   CMake

-   vcpkg

  

## macOS

  

-   Xcode

-   Visual Studio Code

-   CMake

  

## Linux

  

-   Visual Studio Code

-   GCC / G++

-   GDB

-   CMake

  

※ Ubuntu系ディストリビューションで動作確認済みです。

  

------------------------------------------------------------------------

  

# 学習対象

  

ToyLibは以下のような方を対象としています。

  

-   Modern C++を学びたい

-   ゲームプログラミングを学びたい

-   ゲームエンジンの内部構造を理解したい

-   OpenGLやVulkanを使った描画技術を学びたい

-   自分でゲームエンジンを拡張しながら開発したい

## 📦 About this Repository

This repository hosts the **ToyLib core library** together with sample games  
used for development, validation, and experimentation.

---

## 📚 License

This project is licensed under the [MIT License](./LICENSE).

You are free to use ToyLib in:
- personal projects
- educational use
- commercial products

---

## ⚠ Trademark Notice

The name **"ToyLib"** and its logo are unregistered trademarks of **Daisuke Nishimori**.  
They may not be used to represent other projects without explicit permission.

---

## 🤝 Contact & Feedback

Questions, ideas, and suggestions are welcome.  
Please open an issue on the  
[GitHub Issues page](https://github.com/ToyLib/ToyLibGame/issues).
