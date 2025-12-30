#include "StageScene.h"
#include "FieldScene.h"

void StageScene::InitScene()
{
    
    auto a = CreateActor<toy::Actor>();
    auto mesh = a->CreateComponent<toy::SkeletalMeshComponent>();
    mesh->SetMesh(GetApp()->GetAssetManager()->GetMesh("Hero/hero_m.fbx"));
    a->SetScale(0.001f);
    mesh->GetAnimPlayer()->Play(17);
    
    
    toy::PostEffectDesc effectDesc;
    effectDesc.type = toy::PostEffectType::Sepia;
    effectDesc.intensity = 1.0f;
    GetApp()->GetRenderer()->SetPostEffect(effectDesc);
    
    
    auto font = GetApp()->GetAssetManager()->GetFont("Font/rounded-mplus-1c-bold.ttf", 20);
    // メッセージボックス生成
    toy::MessageBoxActor::Desc d;
    d.position  = Vector3(300, 520, 0);
    d.boxSize   = Vector2(680, 180);
    d.padding   = Vector2(18, 14);
    d.bgAlpha   = 0.6f;
    d.lineGapPx = 3;
    d.font      = font;
    d.textColor = Vector3(0.8, 0.8, 0.8);
    
    mMsgActor = CreateActor<toy::MessageBoxActor>(d);

    std::string story = StringUtil::ReadTextFileNormalized("ToyGame/Assets/KitGame/Text/Story.txt");
  
    mMsgActor->Open(story
    );
 
    
    // テスト用メッセージ
/*    mMsgActor->Open(
        "むかしむかし、あるところに、おじいさんとおばあさんが住んでいました。\n おじいさんは山へしばかりに、おばあさんは川へせんたくに行きました。\n　おばあさんが川でせんたくをしていると、ドンブラコ、ドンブラコと、大きな桃が流れてきました。\n「おや、これは良いおみやげになるわ」\n　おばあさんは大きな桃をひろいあげて、家に持ち帰りました。　そして、おじいさんとおばあさんが桃を食べようと桃を切ってみると、なんと中から元気の良い男の赤ちゃんが飛び出してきました。「これはきっと、神さまがくださったにちがいない」\n　子どものいなかったおじいさんとおばあさんは、大喜びです。\n　桃から生まれた男の子を、おじいさんとおばあさんは桃太郎と名付けました。\n　桃太郎はスクスク育って、やがて強い男の子になりました。\n　そしてある日、桃太郎が言いました。",
        []()
        {
            // 閉じたときのコールバック（とりあえずログ）
            SDL_Log("MessageBox closed.");

        }
    );
  */
    
}

void StageScene::Update(float delatTime)
{

}
void StageScene::ProcessInput(const toy::InputState& input)
{
    if (input.IsButtonPressed(toy::GameButton::A) == toy::EPressed)
    {
        if (!mMsgActor->IsOpen())
        {
            RequestChange(std::make_unique<FieldScene>());
        }
    }
}

