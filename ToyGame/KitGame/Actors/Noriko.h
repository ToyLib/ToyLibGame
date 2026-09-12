#pragma once

#include "ToyKit.h"

#include <memory>

//=============================================================================
// Noriko
//  ToyKit の Prefab/Agent/IBehavior を使った最小構成の実例（チュートリアル
//  を兼ねる）。詳しい手順は doc/Rules/ToyKit_Prefab_Manual.md、各クラスの
//  役割は doc/Rules/ToyKit_KitPrefab_KitSignal_Reference.md を参照。
//
//  ── 全体の考え方 ──────────────────────────────────────────
//  ゲーム側のキャラクラス（この Noriko）は toy::Actor を継承しない。
//  代わりに、
//    体   = toy::kit::Creature（Prefab派生。見た目・当たり判定だけを持つ）
//    行動 = toy::kit::IBehavior（後述の FleeBehavior。Noriko.cpp 参照）
//  の2つを toy::kit::Agent<TPrefab> というテンプレートで束ねる。
//  「体」と「行動」を分けておくことで、同じ Creature/Humanoid に別の
//  行動を差し替えたり（Wolf/Shiro は Humanoid+ChaseBehavior）、逆に同じ
//  行動を別の体に使い回したり（ChaseBehavior は Humanoid 専用ではない）
//  できる。
//
//  Noriko クラス自体は Agent<Creature> の「using Agent::Agent;」だけの
//  薄いサブクラス（＝Agent のコンストラクタをそのまま使う）。これは
//  「Noriko」という名前でクラスを扱えるようにするための型名にすぎず、
//  中身の実装は一切書かない（実装は全部 Agent 側・Behavior側にある）。
//=============================================================================
class Noriko : public toy::kit::Agent<toy::kit::Creature>
{
public:
    using Agent::Agent;
};

// Noriko の生成はコンストラクタを直接呼ばず、必ずこの Factory 関数を経由する。
// 理由: Desc の組み立て（MakeNorikoDesc、Noriko.cpp 内）や、行動
// （FleeBehavior）の生成・初期位置の設定など、構築に必要な手順を
// 1箇所にまとめておくため。呼び出し側（Scene）は中身を知らなくてよい。
//
// target には、視界に入ったら逃げ出す相手（Player の Prefab）を渡す
// （Wolf/Shiro が MakeWolf/MakeShiro に target を渡すのと同じ形）。
std::unique_ptr<Noriko> MakeNoriko(toy::Application* app, const Vector3& position, toy::kit::Prefab* target);
