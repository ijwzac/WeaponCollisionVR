#include <SKSE/SKSE.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include "OnMeleeHit.h"
#include "Utils.h"


using namespace OnMeleeHit;
using namespace SKSE;
using namespace SKSE::log;

OriMeleeHit lastMeleeHit;

OnMeleeHitHook& OnMeleeHitHook::GetSingleton() noexcept {
    static OnMeleeHitHook instance;
    return instance;
}



void OnMeleeHitHook::InstallHook() {
    // Taken from: https://github.com/doodlum/MaxsuWeaponSwingParry-ng/blob/main/src/WeaponParry_Hooks.h
    // (with permission from doodlum and Maxsu)
    SKSE::AllocTrampoline(1 << 4);

    REL::Relocation<std::uintptr_t> OnMeleeHitBase{REL::RelocationID(37650, 38603)};
    auto& trampoline = SKSE::GetTrampoline();
    _OnMeleeHit = trampoline.write_call<5>(OnMeleeHitBase.address() + REL::Relocate(0x38B, 0x45A), OnMeleeHit);
}

void OnMeleeHitHook::OnMeleeHit(RE::Actor* hit_causer, RE::Actor* hit_target, std::int64_t a_int1, bool a_bool,
                                void* a_unkptr) {
    //log::trace("Hit causer:{}. Target:{}", hit_causer->GetDisplayFullName(),
    //           hit_target->GetDisplayFullName());
    if (bEnableWholeMod) {
        auto playerAA = RE::PlayerCharacter::GetSingleton();
        if (playerAA) {
            auto playerActor = static_cast<RE::Actor*>(playerAA);
            if (playerActor) {
                log::trace("Hit causer:{}. Target:{}. Causer is player:{}", hit_causer->GetDisplayFullName(), hit_target->GetDisplayFullName(), hit_causer == playerActor);
                if (hit_target == playerActor) {
                    log::trace("Delay a hit to player");
                    // If hitting the player, we record everything we need to call the vanilla _OnMeleeHit, and call it
                    // a few frames later
                    auto h =
                        OriMeleeHit(iFrameCount + iDelayEnemyHit, hit_causer, hit_target, a_int1, a_bool, a_unkptr);
                    meleeQueue.PushCopy(h);
                    return;
                }
                //if (hit_causer == playerActor) { // This is not working, probably because PLANCK changes the hit event
                //    log::trace("Checking if player just had weapon collision so this attack should be nullified");
                //    auto diff = iFrameCount - iFrameLastCollision;
                //    if (diff > 0 && diff < collisionEffectDurPlayerShort) {
                //        log::trace("Nullify player attack");
                //        return;
                //    }
                //}
            }
        }
    }
    

    // Call the normal game's code
    _OnMeleeHit(hit_causer, hit_target, a_int1, a_bool, a_unkptr);
}


bool OnMeleeHit::IsAttacking(const RE::ATTACK_STATE_ENUM attackState) {
    return attackState == RE::ATTACK_STATE_ENUM::kHit || attackState == RE::ATTACK_STATE_ENUM::kSwing;
}

DistResult OnMeleeHit::Dist(const RE::NiPoint3& A, const RE::NiPoint3& B, const RE::NiPoint3& C,
                            const RE::NiPoint3& D) {
    // Check if any point is (0,0,0)
    if (AnyPointZero(A, B, C, D)) {
        RE::NiPoint3 tmp;
        return DistResult(123456.0f, tmp);
    }
    const auto CD = D - C;
    float CD2 = CD * CD;
    auto inPlaneA = A - (CD * (CD * (A - C) / CD2));
    auto inPlaneB = B - (CD * (CD * (B - C) / CD2));
    auto inPlaneBA = inPlaneB - inPlaneA;
    const float inPlaneBA2 = inPlaneBA * inPlaneBA;
    float t;
    if (inPlaneBA2 < 0.00005f)
        t = 0.0f;
    else
        t = inPlaneBA * (C - inPlaneA) / inPlaneBA2;
    auto segABtoLineCD = Lerp(A, B, Clamp01(t));

    return DistResult(dist(A, B, constrainToSegment(segABtoLineCD, C, D)), segABtoLineCD);
}

RE::NiPoint3 OnMeleeHit::Lerp(const RE::NiPoint3& A, const RE::NiPoint3& B, const float k) 
{
    return A + (B - A) * k; 
}

float OnMeleeHit::Clamp01(const float t) 
{ 
    return std::max(0.0f, std::min(1.0f, t)); 
}

RE::NiPoint3 OnMeleeHit::constrainToSegment(const RE::NiPoint3& position, const RE::NiPoint3& a,
                                            const RE::NiPoint3& b) {
    auto ba = b - a;
    auto t = ba * (position - a) / (ba * ba);
    return Lerp(a, b, Clamp01(t));
}

float OnMeleeHit::dist(const RE::NiPoint3& A, const RE::NiPoint3& B, const RE::NiPoint3& C) {
    return constrainToSegment(C, A, B).GetDistance(C);
}

// From: https://github.com/fenix31415/UselessFenixUtils
void OnMeleeHit::play_sound(RE::TESObjectREFR* object, RE::FormID formid) {
    RE::BSSoundHandle handle;
    handle.soundID = static_cast<uint32_t>(-1);
    handle.assumeSuccess = false;
    *(uint32_t*)&handle.state = 0;

    auto manager = RE::BSAudioManager::GetSingleton();
    if (manager) {
        soundHelper_a(manager, &handle, formid, 16);
        if (set_sound_position(&handle, object->data.location.x, object->data.location.y, object->data.location.z)) {
            handle.SetVolume(1.f);
            soundHelper_b(&handle, object->Get3D());
            soundHelper_c(&handle);
        }
    }
}



bool OnMeleeHit::play_impact_1(RE::Actor* actor, const RE::BSFixedString& nodeName) {
    auto root = netimmerse_cast<RE::BSFadeNode*>(actor->Get3D());
    if (!root) return false;
    auto bone = netimmerse_cast<RE::NiNode*>(root->GetObjectByName(nodeName));
    if (!bone) return false;

    float reach = Actor_GetReach(actor) * fRangeMulti;
    auto weaponDirection =
        RE::NiPoint3{bone->world.rotate.entry[0][1], bone->world.rotate.entry[1][1], bone->world.rotate.entry[2][1]};
    RE::NiPoint3 to = bone->world.translate + weaponDirection * reach;
    RE::NiPoint3 P_V = {0.0f, 0.0f, 0.0f};

    return play_impact_2(actor, RE::TESForm::LookupByID<RE::BGSImpactData>(0x0004BB52), &P_V, &to, bone);
}


// From: https://github.com/fenix31415/UselessFenixUtils
bool OnMeleeHit::play_impact_2(RE::TESObjectREFR* a, RE::BGSImpactData* impact, RE::NiPoint3* P_V, RE::NiPoint3* P_from,
                 RE::NiNode* bone) {
    return play_impact_3(a->GetParentCell(), 1.0f, impact->GetModel(), P_V, P_from, 1.0f, 7, bone);
}

bool OnMeleeHit::play_impact_3(RE::TESObjectCELL* cell, float a_lifetime, const char* model, RE::NiPoint3* a_rotation,
                               RE::NiPoint3* a_position, float a_scale, uint32_t a_flags, RE::NiNode* a_target) {
    return RE::BSTempEffectParticle::Spawn(cell, a_lifetime, model, *a_rotation, *a_position, a_scale, a_flags, a_target);
}

PRECISION_API::WeaponCollisionCallbackReturn OnMeleeHit::PrecisionWeaponsCallback(
    const PRECISION_API::PrecisionHitData& a_precisionHitData) {

    auto hit_causer = a_precisionHitData.attacker;
    auto hit_target = a_precisionHitData.target == nullptr ? nullptr : a_precisionHitData.target->As<RE::Actor>();


    PRECISION_API::WeaponCollisionCallbackReturn ret;
    ret.bIgnoreHit = false;
    return ret;
}

bool OnMeleeHit::play_impact_precision(RE::Actor* actor, const RE::BSFixedString& nodeName, RE::NiPoint3& hitPos) {
    auto root = netimmerse_cast<RE::BSFadeNode*>(actor->Get3D());
    if (!root) return false;
    auto bone = netimmerse_cast<RE::NiNode*>(root->GetObjectByName(nodeName));
    if (!bone) return false;

    RE::NiPoint3 P_V = {0.0f, 0.0f, 0.0f};

    return play_impact_2(actor, RE::TESForm::LookupByID<RE::BGSImpactData>(0x0004BB52), &P_V, &hitPos, bone);
}