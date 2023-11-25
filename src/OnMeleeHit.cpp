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
    if (bEnableWholeMod && iDelayEnemyHit > 0) {
        auto playerAA = RE::PlayerCharacter::GetSingleton();
        if (playerAA) {
            auto playerActor = static_cast<RE::Actor*>(playerAA);
            if (playerActor) {
                log::trace("Hit causer:{}. Target:{}. Causer is player:{}", hit_causer->GetDisplayFullName(), hit_target->GetDisplayFullName(), hit_causer == playerActor);
                if (hit_target == playerActor) {
                    log::trace("Delay a hit to player");
                    // If hitting the player, we record everything we need to call the vanilla _OnMeleeHit, and call
                    // it a few frames later
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
        return DistResult();
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

RE::NiPoint3 closestPointOnSegmentForPoint(RE::NiPoint3& segStart, RE::NiPoint3& segEnd, RE::NiPoint3& target) {
    RE::NiPoint3 start2End = segEnd - segStart;
    RE::NiPoint3 start2Target = target - segStart;
    RE::NiPoint3 result;

    float c1 = start2Target.Dot(start2End);
    if (c1 <= 0) {
        result = segStart;
        return result;
    }

    float c2 = start2End.Dot(start2End);
    if (c2 <= c1) {
        result = segEnd;
        return result;
    }

    float b = c1 / c2;
    result = segStart + start2End * b;
    return result;
}

float distPoint2Segment(RE::NiPoint3& segStart, RE::NiPoint3& segEnd, RE::NiPoint3& target) {
    RE::NiPoint3 p = closestPointOnSegmentForPoint(segStart, segEnd, target);
    return p.GetDistance(target);
}

RE::NiPoint3 projectPointOntoCirclePlance(RE::NiPoint3& circleCenter, RE::NiPoint3& circleNormal, RE::NiPoint3& target) {
    RE::NiPoint3 toPoint = target - circleCenter;
    float distToPlane = toPoint.Dot(circleNormal);
    return target - circleNormal * distToPlane;
}

DistResult DistForShield(RE::NiPoint3& shieldCenter, RE::NiPoint3& shieldNormal, float radius, RE::NiPoint3& weapP1,
                         RE::NiPoint3& weapP2) {
    // Project the weapon to the plane of the circle
    RE::NiPoint3 weapProjP1 = projectPointOntoCirclePlance(shieldCenter, shieldNormal, weapP1);
    RE::NiPoint3 weapProjP2 = projectPointOntoCirclePlance(shieldCenter, shieldNormal, weapP2);
    RE::NiPoint3 weapProj = weapProjP2 - weapProjP1;

    // Find the closest point on projected weapon to the circleCenter
    RE::NiPoint3 closestPointWeapProj2Center = closestPointOnSegmentForPoint(weapProjP1, weapProjP2, shieldCenter);

    // Check if the closest point is inside the circle
    if (closestPointWeapProj2Center.GetDistance(shieldCenter) <= radius) {
        // Inside, then we only need to calculate the distance between the original weapon and part of the projected weapon (the part that's within the circle)
        // Instead of an accurate result, we just divide the projected weapon into 30 points, 
        //    ignore points not in the circle, and return the minimal dist between the circle to the segment
        RE::NiPoint3 contactPoint;
        float shortestDist = 9999.0f;
        for (int i = 0; i < 30; i++) {
            RE::NiPoint3 p = weapProjP1 + weapProj / 30 * i;
            if (p.GetDistance(shieldCenter) < radius) {
                float pDist = distPoint2Segment(weapP1, weapP2, p);
                if (pDist < shortestDist) {
                    shortestDist = pDist;
                    contactPoint = p;
                }
            }
        }

        return DistResult(shortestDist, contactPoint, true); // since shield is on left hand, we can directly put true here

    } else {
        // Outside, then we get the point that is on the circle edge and closest to projected weapon, and return the dist from this point to weapon
        RE::NiPoint3 direction = closestPointWeapProj2Center - shieldCenter;
        direction = direction / direction.Length();
        RE::NiPoint3 closestPointOnCircle = shieldCenter + direction * radius;

        float dist = distPoint2Segment(weapP1, weapP2, closestPointOnCircle);
        return DistResult(dist, closestPointOnCircle, true);
    }
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
