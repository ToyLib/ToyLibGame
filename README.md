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

ToyLibは、***Modern C++（C++20）でゲームプログラミングを学ぶための軽量ゲームエンジン／クラスライブラリ***です。

「***ゲームプログラマのオモチャ***」をコンセプトに、ソースコード全体を理解できる規模を維持しながら、本格的な3Dゲームを開発できます。

巨大なゲームエンジンをブラックボックスとして利用するのではなく、自ら改造し、仕組みを理解しながらゲーム開発を楽しむことを目指しています。

---

## 📁 リポジトリ構成

| ディレクトリ | 内容 |
|---|---|
| `ToyLib/` | エンジン本体（Actor/Component、描画、物理、音、UI など） |
| `ToyKit/` | ゲーム開発用テンプレート（シーン管理、Prefab、Behavior、Signal） |
| `ToyGame/` | サンプルゲーム（`KitGame` / `RPG`）とそのアセット・設定 |
| `doc/` | 設計ルール・利用ガイド、CMake / VSCode 設定テンプレート |
| `tools/` | 開発者向け補助ツール（設定エディタ、glTFアニメーション一覧） |
| `toylib_api_reference.md` | 主要クラスの API リファレンス |

名前空間は `toy::`（ToyKit は `toy::kit::`）です。

---

# ライブラリ構成

## ToyLib（基本ライブラリ）

ゲームエンジンとして共通に利用する機能を提供します。

### Engine Core

- Actor / Component ベースの構成
- `Application` を継承してゲームを作り、`TOYLIB_REGISTER_APP` で起動アプリを登録
- 入力システム（キーボード / ゲームパッド、JSON によるボタン割り当て）
- 昼夜サイクル（TimeOfDaySystem）

### Rendering

- **OpenGL / Vulkan のデュアルバックエンド**（`Application_Settings.json` の `renderer_backend` で `GL` / `VK` を切り替え。ゲームコードは共通）
- スケルタルアニメーション対応3Dモデル（glTF 推奨 / FBX / DirectX X など、Assimp 経由）
- シャドウマッピング
- ポイントライト、時間帯によるライティング変化、フォグ
- GPU パーティクル（JSON で定義）
- スプライト / テキスト / ビルボード / 9スライススプライト
- シーンキャプチャ、レンダーサーフェス
- ポストエフェクト（Sepia / CRT / FeilyLand / Noisy / Grayscale / Monochrome / Watercolor / OldFilm、最大2段チェーン）

### Environment

- スカイドーム
- 天候表現（雨・雪などのドーム / スクリーンオーバーレイ）、積雪フィールド

### Camera

- 追従カメラ / オービットカメラ / 定点カメラ
- CameraManager によるカメラ切り替え

### Movement

- 方向移動 / FPS移動 / 追従移動 / 慣性移動 / オービット移動

### Physics

- 衝突判定（OBB 等のバウンディングボリューム、レーザーコライダー）
- 重力処理、地形衝突、壁判定
- 距離・方角センサー

### Audio

- 3Dサウンド（OpenAL Soft）
- BGMストリーミング再生（Ogg Vorbis）
- サウンドミキサー

### UI

- UIスタックによる入力の配送、アンカー付きレイアウト
- メッセージウィンドウ（折り返し・改ページ・文字送り）
- 選択リスト、選択肢付きメッセージ、確認ダイアログ
- UI の Tween アニメーション
- UI 表示中のシーンポーズ
- 枠・背景・カーソルはコード生成テクスチャで描画（画像アセット不要）

### Utility

- JSONによる各種設定（アプリケーション / レンダラー / 入力 / パーティクル）
- デバッグ描画・デバッグオーバーレイ・統計表示
- ログ出力、数学・イージング・フラスタム関連ユーティリティ

---

## ToyKit（ゲームテンプレート）

ゲーム開発を素早く始めるためのテンプレートライブラリです。

- **KitCore** — シーン（`IScene`、Environment / World / SpawnCharacters / UI の4フェーズ）、シーン遷移（`GameFlow`）、ステートマシン
- **KitPrefab** — Humanoid / Creature / Projectile / StaticObject などの Actor テンプレートと配置
- **KitBehavior** — `Agent` + `IBehavior` による振る舞いの組み立て（Chase / Flee など）
- **KitSignal** — ゲームイベントを扱うシグナル

---

## ToyGame（サンプルゲーム）

- **KitGame** — ToyKit を使ったサンプル（タイトル / ストーリー / フィールド / 雪原シーン）
- **RPG** — ToyLib を直接使ったサンプル

起動するゲームは [`ToyGame/RegiterGame.cpp`](./ToyGame/RegiterGame.cpp) の `TOYLIB_REGISTER_APP(...)` で切り替えます。

---

# クロスプラットフォーム

ToyLibは、同じゲームコードを

- Windows
- macOS
- Linux

でそのままビルド・実行できる **完全なソース互換** を目標としています。

プラットフォーム固有のコードを書くことなく、各OSでゲームを開発できます。

---

# 使用ライブラリ

- SDL3 / SDL3_image / SDL3_ttf
- OpenGL（glad）
- Vulkan（シェーダーは `glslc` で SPIR-V にビルド時コンパイル）
- Assimp
- OpenAL Soft
- stb_vorbis
- nlohmann/json

---

# ビルド

CMake 3.15 以上と C++20 対応コンパイラが必要です。Vulkan SDK と `glslc`（shaderc）も必要です。

`PrepareRun` ターゲットはゲーム本体のビルドに加えて、シェーダー・アセット・設定ファイルを実行ディレクトリへコピーします。実行時はその出力ディレクトリをカレントディレクトリにしてください。

## Windows

- Visual Studio 2026 / Visual Studio Code
- CMake
- vcpkg（`C:/tools/vcpkg` を想定）

```bash
vcpkg install sdl3 sdl3-image sdl3-ttf assimp openal-soft shaderc --triplet x64-windows
```

```bash
cmake --preset windows
```

```bash
cmake --build --preset windows-debug-run
```

実行ファイルは `build/Debug/GameApp.exe`（Release は `build/Release/`）に出力されます。

## macOS

- Xcode（`ToyLibGame.xcodeproj`）/ Visual Studio Code
- CMake
- Homebrew

```bash
brew install sdl3 sdl3_image sdl3_ttf assimp openal-soft shaderc vulkan-loader molten-vk
```

```bash
cmake -B build && cmake --build build --target PrepareRun
```

## Linux

- Visual Studio Code
- GCC / G++、GDB
- CMake

SDL3 系のシステムパッケージが無い場合は、`ToyLib/External/SDL3` などにソースを置くと自動でソースビルドにフォールバックします（`TOYLIB_LINUX_USE_VENDORED_SDL3`）。

```bash
cmake -B build && cmake --build build --target PrepareRun
```

※ Ubuntu系ディストリビューションで動作確認済みです。

macOS / Linux では実行ファイルは `build/GameApp` に出力されます。VSCode 用の設定テンプレートは [`doc/vscode_templates/`](./doc/vscode_templates/) にあります。

---

# 設定ファイル

| ファイル | 内容 |
|---|---|
| `ToyLib/Settings/*.json` | アプリケーション・レンダラー・入力のデフォルト設定 |
| `ToyGame/Settings/System/*.json` | 環境ごとの上書き設定（git 管理外。[`tools/settings_editor.py`](./tools/settings_editor.py) で生成・編集） |
| `ToyGame/Settings/*.json` | ゲーム固有の設定（パーティクル、キャラクターなど） |

---

# ドキュメント

- [API リファレンス](./toylib_api_reference.md)
- [ToyKit ゲームアーキテクチャ概要](./doc/Rules/ToyKit_Game_Architecture_Overview.md)
- [ToyKit Prefab マニュアル](./doc/Rules/ToyKit_Prefab_Manual.md)
- [KitPrefab / KitSignal リファレンス](./doc/Rules/ToyKit_KitPrefab_KitSignal_Reference.md)
- [Actor ライフサイクルのルール](./doc/Rules/ToyKit_Actor_Lifecycle_Rules.md)
- [ToyKit 設計ガイドライン](./doc/Rules/ToyKit_Design_Guidelines_Revise.md)
- [UI 利用ガイド](./doc/Rules/ToyLib_UI_Guide.md)
- [ポストエフェクト利用ガイド](./doc/Rules/ToyLib_PostEffect_Guide.md)

---

# 学習対象

ToyLibは以下のような方を対象としています。

- Modern C++を学びたい
- ゲームプログラミングを学びたい
- ゲームエンジンの内部構造を理解したい
- OpenGLやVulkanを使った描画技術を学びたい
- 自分でゲームエンジンを拡張しながら開発したい

---

## 📦 About this Repository

This repository hosts the **ToyLib core library** and the **ToyKit** game template, together with sample games
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
