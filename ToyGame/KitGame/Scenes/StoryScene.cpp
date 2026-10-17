#include "StoryScene.h"
#include "FieldScene.h"

void StoryScene::DefineWorld()
{
    auto a = CreateActor<toy::Actor>();
    auto mesh = a->CreateComponent<toy::SkeletalMeshComponent>();
    mesh->SetMesh(GetApp()->GetAssetManager()->GetMesh("Hero/hero_m.fbx"));
    a->SetScale(0.001f);
    a->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    Quaternion q = Quaternion::Identity;
    a->SetRotation(q);
    mesh->GetAnimPlayer()->Play(17);

    toy::PostEffectDesc effectDesc;
    effectDesc.stage0.type = toy::PostEffectType::Sepia;
    effectDesc.stage0.intensity = 1.0f;
    effectDesc.stage1.type = toy::PostEffectType::OldFilm;
    effectDesc.stage1.intensity = 1.0f;
    GetApp()->GetRenderer()->SetPostEffect(effectDesc);

    
    auto font = GetApp()->GetAssetManager()->GetFont("Font/rounded-mplus-1c-bold.ttf", 20);
    // メッセージボックス生成
    toy::MessageBoxActor::Desc d;
    d.layout.anchor = toy::UIAnchor::Bottom;
    d.layout.offset = Vector2(0, -20);
    d.layout.size   = Vector2(680, 180);
    d.padding = Vector2(18, 14);
    d.frame.bgAlpha = 0.6f;
    d.lineGapPx = 3;
    d.font = font;
    d.textColor = Vector3(0.8, 0.8, 0.8);

    mMsgActor = CreateActor<toy::MessageBoxActor>(d);

    // メッセージに添える選択肢（位置はメッセージボックスが右上に合わせる）
    toy::ChoiceListActor::Desc choiceDesc;
    choiceDesc.font = font;
    choiceDesc.layout.size = Vector2(220.0f, 0.0f);   // 高さは項目数から自動
    choiceDesc.allowCancel = false;                   // どちらかを必ず選ばせる
    mMsgActor->AttachChoiceList(CreateActor<toy::ChoiceListActor>(choiceDesc));

    mStory = StringUtil::ReadTextFileNormalized("ToyGame/Assets/KitGame/Text/Story.txt");
    OpenStory();

    // テスト用スプライト
    auto ac = CreateActor<toy::Actor>();
    auto sp = ac->CreateComponent<toy::SpriteComponent>(1000);
    sp->SetTexture(GetApp()->GetAssetManager()->GetTexture("UI/target3.png"));
    ac->SetPosition(Vector3(100.0f, 100.0f, 0));

    // 定点カメラ
    auto cameraActor = CreateActor<toy::Actor>();
    auto cameraComp = cameraActor->CreateComponent<toy::FixedCameraComponent>();
    cameraComp->LookAt(Vector3(0.0f, 1.5f, -3.0f), Vector3(0.0f, 1.5f, 0.0f));
    GetApp()->GetCameraManager()->SetActiveCamera(cameraComp);
    
    DeploySky();
    
}

void StoryScene::Update(float delatTime)
{
}
void StoryScene::ProcessInput(const toy::InputState &input)
{
}

// 読み終えたら（最後のページで）選択肢を出す
void StoryScene::OpenStory()
{
    mMsgActor->OpenWithChoices(mStory, { "次へ進む", "もう一度読む" },
                               [this](int index)
                               {
                                   if (index == 0)
                                   {
                                       RequestChange(std::make_unique<FieldScene>());
                                   }
                                   else
                                   {
                                       OpenStory();
                                   }
                               });
}


void StoryScene::DeploySky()
{
    // スカイドーム
    auto skyActor = CreateActor<toy::Actor>();
    auto dome = skyActor->CreateComponent<toy::WeatherDomeComponent>();
    // オーバーレイ
    auto overlay = skyActor->CreateComponent<toy::WeatherOverlayComponent>();

    GetApp()->GetTimeOfDaySystem()->SetTimeScale(000.0f);
    GetApp()->GetTimeOfDaySystem()->SetTime(21.0f, 0.0f);
    
    mWeather = std::make_unique<toy::WeatherManager>();
    mWeather->SetWeatherDome(dome);
    mWeather->SetWeatherOverlay(overlay);
    mWeather->ChangeWeather(toy::WeatherType::STORM);
}
