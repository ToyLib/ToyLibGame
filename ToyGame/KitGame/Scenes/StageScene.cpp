#include "StageScene.h"

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
    
    
    // メッセージボックス生成
    auto msgActor = CreateActor<toy::MessageBoxActor>();

    // Actorの位置で文字位置を調整（背景左上基準）
    msgActor->SetPosition(Vector3(40.0f, 520.0f, 0.0f));

    //mMessageBox = msgActor.get();
    //AddActor(std::move(msgActor));

    // テスト用メッセージ
    msgActor->Open(
        "むかしむかし、あるところに、おじいさんとおばあさんが住んでいました。　おじいさんは山へしばかりに、おばあさんは川へせんたくに行きました。　おばあさんが川でせんたくをしていると、ドンブラコ、ドンブラコと、大きな桃が流れてきました。「おや、これは良いおみやげになるわ」　おばあさんは大きな桃をひろいあげて、家に持ち帰りました。　そして、おじいさんとおばあさんが桃を食べようと桃を切ってみると、なんと中から元気の良い男の赤ちゃんが飛び出してきました。「これはきっと、神さまがくださったにちがいない」　子どものいなかったおじいさんとおばあさんは、大喜びです。　桃から生まれた男の子を、おじいさんとおばあさんは桃太郎と名付けました。　桃太郎はスクスク育って、やがて強い男の子になりました。　そしてある日、桃太郎が言いました。",
        []()
        {
            // 閉じたときのコールバック（とりあえずログ）
            SDL_Log("MessageBox closed.");
        }
    );
    
    
}

void StageScene::Update(float delatTime)
{
    std::cout << "Scene Stage " << delatTime << std::endl;
}
