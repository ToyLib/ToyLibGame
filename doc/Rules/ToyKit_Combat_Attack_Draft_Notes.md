# Behavior攻撃アクション設計 ドラフトメモ（未確定）

> これは2026-09-14の検討会話の途中経過メモです。**まだ実装方針として確定していません**。
> 慎重に進めたいという方針のため、次に着手する前に一度見直すこと。

## 実装進捗

- **2026-09-14: 「Playerの攻撃をNinja/Bunnyが検知する」最小実装まで完了・動作確認済み。**
  `Prefab::TakeDamage(int)`(仮想関数、既定no-op) / `Humanoid`への攻撃用コライダー追加
  (`C_HITBOX`+チームフラグ、`SetAttackColliderActive()`で有効/無効切替) /
  `Humanoid`が自分の`OnCollision()`にConnectして`HandleCollision()`でチーム判定→`TakeDamage()`呼び出し、
  という形で実装。Bunny/Ninja/Skirmisher側は無変更（`Prefab::OnCollision`の双方向衝突登録により自動検知）。
  実機で攻撃時に`[Humanoid] ... took 1 damage`のログ出力を確認済み。
  変更ファイル: `ToyKit/include/KitPrefab/Prefab.h`, `HumanoidDesc.h`, `Humanoid.h/.cpp`,
  `ToyGame/KitGame/Actors/Player.cpp`。
  ダメージ量はまだ固定値(`1`)、`TakeDamage`の中身も検知確認用のprintfのみ。HP等の実際の処理は未着手。

## 背景・目的

Playerが持つLockon機構を発展させ、Behavior（AI制御キャラクター、例: Bunny）側からも
「攻撃してくる」アクションを追加したい。現状は以下の通り:

- Lockon（`Humanoid::SearchTarget()` 等、[ToyKit_Prefab_Manual.md](ToyKit_Prefab_Manual.md) 参照）はPlayer/NPC共通クラス`Humanoid`に実装済みで、Player専用ではない。
- Player側の攻撃は現状「アニメ再生 + 移動ロック（`SetMovable(false)`）」のみで、ダメージ判定・HPは未実装。
- `ChaseBehavior`には「近づいた後どうするか（攻撃）は未定」という設計コメントが既にあり、Behavior攻撃の空白地点として認識されていた。
- `Prefab`はゲーム上の意味（HPなど）を持たない設計方針（[ToyKit_Design_Guidelines_Revise.md](ToyKit_Design_Guidelines_Revise.md) 参照）。

## ここまでの検討で合意した方向性

1. **Lockon流用**
   `Humanoid::SearchTarget()` / センサーベースの候補収集ロジックは、Player/NPC共通の実装なので、
   Behaviorのターゲット捕捉（誰を攻撃対象にするか）にもそのまま流用できそう。

2. **攻撃Behavior**
   `ChaseBehavior`に「近づいたら攻撃」の状態（Attackステート）を追加し、
   Playerの`PlayerControlBehavior::InputAttack()`と同様に、
   アニメ再生 + 一定時間の移動ロックで攻撃モーションを表現する案。
   → 当たり判定をどこで発火させるか（Collider同士のCollisionEventか、専用のAttackRange判定か）は未検討・次の論点。

3. **被ダメージの共通層は「HP」を持たない**
   ToyKit / 共通インターフェース側は、「HP」という概念そのものを持たず、
   最小限の合図だけを持つイメージ。

   ```cpp
   // イメージ（未確定）
   TakeDamage(int amount);   // ダメージが来たことを伝えるだけ
   IsDefeated() const;       // 撃破済みかどうか
   ```

   - 途中の議論で「ここでいうHP（共通インターフェースが扱う量）は、
     ゲームデザイン上プレイヤーに見せる『HP』と必ずしも一致しない」という指摘が出た。
     単純にHPを引く実装もあれば、シールドを先に減らす、よろけゲージに変換するだけでHPという表示は存在しない、
     といった実装もあり得るため、共通層は「ダメージが来た/撃破された」という事実だけを知っていればよい。

4. **パラメータ変化はPrefab派生クラスの自由**
   `TakeDamage()`を受けて実際に何がどう変化するか（HPを引く／シールドを削る／よろけゲージにする等）は、
   各Prefab派生クラス（`Player`, `Skirmisher`等）が自由に実装する。
   すばやさ・かしこさ・MPのような他のゲーム固有パラメータも、共通コンポーネント化はせず、
   各ゲーム（KitGame / RPG）が自分の`Desc`構造体にプリミティブなフィールドとして自由に足す方針
   （[[feedback_incremental_migration_workflow.md]]的な「2つ目の利用箇所が出るまで一般化しない」考え方）。

   - 例外的に、HP絡み（撃破判定・被弾リアクション・将来のHPバーUI等）はKitGame/RPG双方で
     同じ形が欲しくなる可能性が高いため、`TakeDamage`/`IsDefeated`程度の
     最小限インターフェース（規約レベルでも可）だけ最初から用意しておく価値がありそう、という意見が出ている。

## 攻撃手段によるBehavior分割（近接 / Range）

- 近接攻撃とRange攻撃では「攻撃を仕掛けるかどうかの判断ロジック」自体が大きく異なる
  （近接は`ChaseBehavior`のように詰め寄る、Rangeは`FleeBehavior`寄りに距離を保ちつつ発射）ため、
  1つのBehaviorにフラグ分岐で詰め込まず、最初から別クラスにする方針。
- 両者とも最終的には相手の`HurtBox`に何かが接触して`TakeDamage`が呼ばれる点は共通。
  違いは「何が接触判定を担うか」:
  - **近接**: Behavior自身（の専用攻撃コライダー）が直接相手の`HurtBox`に接触 → 既存の`CollisionEvent`をそのまま使う
  - **Range**: 別Prefab/Actor（飛び道具、自前のHitBox持ち）を生成し、その飛び道具側が相手の`HurtBox`との`CollisionEvent`を持つ
    → ダメージ適用の末端は結局同じ経路に合流する
- 「共通層はTakeDamageの合図だけ持つ」という既存方針と矛盾なく繋がる。

## 近接攻撃用コライダーは本体コライダーと別に持つ

- 近接攻撃の「当たる場所・範囲」を決めるコライダーは、移動・押し出しに使う本体コライダーとは別に持つ方針。
  理由:
  - 本体コライダーは常時有効である必要があるが、攻撃判定は攻撃モーション中の一部フレームだけ有効にしたい（ライフサイクルが異なる）
  - 攻撃の間合い・形状（武器のリーチ等）は本体の当たり判定形状と一致するとは限らない
- `ColliderComponent::SetEnabled(bool)`（`ToyLib/include/Physics/ColliderComponent.h:93`）が既にあるため、
  専用の攻撃コライダーを`C_HITBOX`フラグ付きで用意し、普段は`SetEnabled(false)`、
  攻撃アニメの有効フレームの間だけ`true`にする、という運用が実装上は無理なくできそう。
- **発火タイミングの方針**: アニメーションのキーフレームと厳密にリンクさせるのは
  （アニメシステム側にイベント/コールバック機構が必要になり）大変なので見送り、
  代わりに「攻撃コライダーの位置・大きさ」を攻撃種別ごとに事前セットしておき、
  「攻撃アクション開始からの経過時間」で発火タイミングも攻撃種別ごとに個別指定する方針とする
  （例: `hitStartSec`/`hitEndSec`のような秒数指定）。
  - `Humanoid::UpdateMovableRecovery()`(Humanoid.cpp:404-413)が既に
    「攻撃アニメ開始からの経過時間を見て`mMovable`を戻す」という同種の時間ベース管理をしているため、
    同じ経過時間トラッキングに相乗りできそう。
  - 留意点: アニメ再生速度が可変になった場合（コンボ速度バフ等）は秒数指定も連動調整が必要になるが、
    現状は固定速度前提のため問題ない想定。

## TakeDamage / 被弾判定の受け方（実装方針が固まった）

近接攻撃の最小実装に向けて、以下3点が決まった。

1. **`TakeDamage`は`Prefab`の仮想関数として持たせる**（別途`Attackable`インターフェースは作らない）。
   `SetVisible`/`PlayAnimation`等、既存の他の仮想関数と同じ並びに追加するイメージ。

   ```cpp
   // Prefab.h イメージ（未確定）
   virtual void TakeDamage(int amount) {}
   ```

   - 相手の識別（フレンドリーファイア防止＝誰の攻撃なら有効か）は、新規のフラグ体系
     （`C_HITBOX`/`C_HURTBOX`）を持ち出すのではなく、既存の`visionTargetMask`
     （`CreatureDesc.h:53` / `HumanoidDesc.h:88`、例: `C_PLAYER_TEAM`）と同じ
     チームフラグの仕組みを流用して判定する方針。

2. **ダメージ量は`TakeDamage(int amount)`の引数として渡せば十分**。
   受け取った側は引数を見れば済むので、別途「どれだけ受けたか」を問い合わせる手段は不要。

3. **被弾判定（攻撃コライダーとの衝突検知→`TakeDamage`呼び出し）はPrefab派生クラス自身が受ける方が直感的**。
   `Prefab::OnCollision()`(`Signal<CollisionEvent>`, Prefab.h:70)には現状`Agent`が接続して
   `IBehavior::OnCollision`に転送する経路があるが、被弾リアクションはBehavior経由にせず、
   Player/Skirmisher等のPrefab派生クラス自身が直接`OnCollision()`にConnectして
   自前の`TakeDamage()`実装を呼ぶ形にする。
   → Behaviorは「攻撃するかどうかの意思決定」に専念し、「攻撃を受けた時にどうなるか」は
     キャラクタークラス自身の責務として分離される（既存の「意味づけはGame Logic側」方針に合致）。

- **未確定のまま残る点**: `IsDefeated()`（撃破判定）をどう表現するかは今回は触れていない。
  `TakeDamage`と同様にPrefabの仮想関数にするか、各派生クラスが独自に持つかは次回検討。

## 攻撃バリエーションはDescで表現（パンチ・剣・キック等）

- Behaviorクラス自体は1つのまま（例: 同じ`MeleeAttackBehavior`）で、
  渡す`Desc`を変えるだけでパンチ・剣・キックのようなバリエーションを作れるようにする方針。
- 前段で決めた「攻撃コライダーの位置・大きさ」「発火タイミング(`hitStartSec`/`hitEndSec`)」は
  そのまま攻撃バリエーションを表すDesc構造体のフィールドとして切り出す。

  ```cpp
  // イメージ（未確定）
  struct MeleeAttackDesc {
      int    animId;          // 再生するアニメーション（Punch/Slash/Kick等）
      Vector3 colliderOffset; // 攻撃コライダーの位置（本体からの相対）
      Vector3 colliderSize;   // 攻撃コライダーの大きさ
      float  hitStartSec;     // 攻撃開始からの発火タイミング
      float  hitEndSec;
      int    damage;          // TakeDamage(amount) に渡すダメージ量
  };
  ```

- `HumanoidDesc`/`ChaseBehaviorDesc`と同じ「プリミティブなDescをゲーム側の工場関数で組み立てる」パターンに乗るため、
  設計方針16（Desc primitive + JSON化）にも自然に沿う。
- 将来的に1体のキャラが複数の`MeleeAttackDesc`を持って状況に応じて選ぶ、という拡張も同じ形のまま乗せられそう
  （ただし現時点では単一Desc想定で十分、複数選択ロジックは未検討）。

## 未決定・次に詰める論点

- `IsDefeated()`（撃破判定）の表現方法（`TakeDamage`と同様にPrefabの仮想関数にするか、各派生クラス独自にするか）
- HP以外のパラメータ（すばやさ・かしこさ・MP）を将来的にDesc/JSON化するかどうか
  （[ToyKit_Prefab_Manual.md](ToyKit_Prefab_Manual.md) 記載の通り、Creature/Humanoid系のDesc化自体がまだ未着手）
- Scene所有の外部テーブル案は検討したが、Actor破棄時のダングリングポインタ後始末が必要になるため見送り、
  Prefab派生クラスにメンバとして持たせる案を優先することにした（ただし最終確定ではない）
