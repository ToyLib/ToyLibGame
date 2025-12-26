# Actor 生成と寿命管理ルール（ToyKit / ToyLib）

## 基本ルール（最重要）

**Scene（およびゲームコード）で Actor を生成する場合は、必ず `CreateActor()` を使用し、  
戻り値の `Actor*` のみを保持する。**

- Scene は Actor を「所有」しない
- Actor の所有権と寿命管理は **Application に一元化**される

---

## なぜ `Actor*` だけを保持してよいのか

ToyLib では Actor の実体は Application が次の形で保持します。

```cpp
std::vector<std::unique_ptr<Actor>> mActors;
```

`CreateActor()` は：

1. `unique_ptr<Actor>` を生成する
2. `Application::AddActor()` に渡す
3. **生ポインタ（Actor\*）のみを返す**

```cpp
Actor* player = CreateActor<PlayerActor>();
```

この `Actor*` は：

- 所有権を持たない参照
- Scene の生存期間中は有効
- 破棄は Application が責任を持つ

---

## Scene 側で許可される保持方法

### OK（推奨）

```cpp
class StageScene : public IScene
{
protected:
    Actor* mPlayer = nullptr;

    void InitScene() override
    {
        mPlayer = CreateActor<PlayerActor>();
    }
};
```

- `Actor*` は参照として保持
- Scene の寿命と一致して使用

---

### NG（禁止）

```cpp
std::unique_ptr<Actor> mPlayer;  // 所有権の奪取（禁止）
std::shared_ptr<Actor> mPlayer;  // 二重管理（禁止）
delete mPlayer;                  // 即時解放（禁止）
```

理由：

- Application の `unique_ptr` 管理と競合する
- Update / Input / Draw 中の削除でクラッシュを招く
- Scene 遷移時の安全性が崩れる

---

## Actor の破棄ルール

### Destroy は「破棄予約」

```cpp
actor->DestroyActor();
```

- 即 delete は行われない
- `Actor::State::Dead` が立つだけ

Application 側で、**安全なタイミング**で回収される：

```cpp
mActors.erase(
    std::remove_if(
        mActors.begin(),
        mActors.end(),
        [](const std::unique_ptr<Actor>& actor)
        {
            return actor->GetState() == Actor::State::Dead;
        }
    ),
    mActors.end()
);
```

---

## Actor ポインタの有効期間

- `CreateActor()` が返す `Actor*` は **Scene が有効な間のみ有効**
- `IScene::Unload()` 後は **すべて無効**

### 禁止例

```cpp
// Scene Unload 後
mPlayer->SetPosition(...); // 未定義動作
```

---

## 設計意図（短文版）

> ToyLib / ToyKit では、  
> **Actor の所有権と寿命管理を Application に集約し、  
> Scene やゲームコードは Actor* の参照のみを扱う。**  
>  
> これにより、更新中・遷移中でも安全な生成と破棄が保証される。

---

## 役割分担まとめ

| 役割 | クラス | 内容 |
|---|---|---|
| Actor の所有 | Application | `std::unique_ptr<Actor>` |
| 生成 | Application / IScene | `CreateActor()` |
| 参照 | Scene / Game | `Actor*` |
| 破棄要求 | Scene / Actor | `DestroyActor()` |
| 実体削除 | Application | Dead 回収 |

---

このルールに従うことで、Scene 遷移・更新中の Actor 管理が安全かつ一貫したものになります。
