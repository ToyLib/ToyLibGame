#pragma once
//
// ToyLib.h
//  - ゲーム側から ToyLib を利用するための統合ヘッダー
//  - 基本的にはこのファイルだけをインクルードすればOK
//
//  ※エンジン内部（ToyLibCore 側）は個別ヘッダを直接インクルードする想定
//

//======================================
// Engine Core
//======================================
#include "Engine/Core/Application.h"
#include "Engine/Core/ApplicationEntry.h"
#include "Engine/Core/Actor.h"
#include "Engine/Core/Component.h"

//======================================
// Engine Runtime
//======================================
#include "Engine/Runtime/InputSystem.h"
#include "Engine/Runtime/AnimationPlayer.h"
#include "Engine/Runtime/TimeOfDaySystem.h"
#include "Engine/Runtime/SingleInstance.h"

//======================================
// Engine Render
//======================================
#include "Engine/Render/Renderer.h"
#include "Engine/Render/Shader.h"
#include "Engine/Render/LightingManager.h"

//======================================
// Asset
//======================================

// Asset Manager
#include "Asset/AssetManager.h"

// --- Animation Assets ---
#include "Asset/Animation/AnimationClip.h"

// --- Audio Assets ---
#include "Asset/Audio/Music.h"
#include "Asset/Audio/SoundEffect.h"

// --- Font Assets ---
#include "Asset/Font/TextFont.h"

// --- Geometry Assets ---
#include "Asset/Geometry/Bone.h"
#include "Asset/Geometry/Mesh.h"
#include "Asset/Geometry/VertexArray.h"
#include "Asset/Geometry/Polygon.h"

// --- Material Assets ---
#include "Asset/Material/Material.h"
#include "Asset/Material/Texture.h"

//======================================
// Camera
//======================================
#include "Camera/CameraComponent.h"
#include "Camera/FollowCameraComponent.h"
#include "Camera/OrbitCameraComponent.h"

//======================================
// Graphics (Visual Components)
//======================================

// 基底コンポーネント
#include "Graphics/VisualComponent.h"

// --- Mesh 系 ---
#include "Graphics/Mesh/MeshComponent.h"
#include "Graphics/Mesh/SkeletalMeshComponent.h"

// --- Sprite / Billboard 系 ---
#include "Graphics/Sprite/SpriteComponent.h"
#include "Graphics/Sprite/BillboardComponent.h"
#include "Graphics/Sprite/TextSpriteComponent.h"
#include "Graphics/Sprite/TextBillboardComponent.h"

// --- Effect 系 ---
#include "Graphics/Effect/ParticleComponent.h"
#include "Graphics/Effect/ShadowSpriteComponent.h"
#include "Graphics/Effect/WireframeComponent.h"

//======================================
// Environment (Sky, Weather)
//======================================
#include "Environment/SkyDomeComponent.h"
#include "Environment/SkyDomeMeshGenerator.h"
#include "Environment/WeatherDomeComponent.h"
#include "Environment/WeatherManager.h"
#include "Environment/WeatherOverlayComponent.h"

//======================================
// Movement
//======================================
#include "Movement/MoveComponent.h"
#include "Movement/DirMoveComponent.h"
#include "Movement/FollowMoveComponent.h"
#include "Movement/FPSMoveComponent.h"
#include "Movement/InertiaMoveComponent.h"
#include "Movement/OrbitMoveComponent.h"

//======================================
// Physics
//======================================
#include "Physics/BoundingVolumeComponent.h"
#include "Physics/ColliderComponent.h"
#include "Physics/GravityComponent.h"
#include "Physics/LaserColliderComponent.h"
#include "Physics/PhysWorld.h"

//======================================
// Audio (再生系)
//======================================
#include "Audio/SoundComponent.h"
#include "Audio/SoundMixer.h"

//======================================
// Utils
//======================================
#include "Utils/MathUtil.h"
#include "Utils/JsonHelper.h"
#include "Utils/StringUtil.h"


