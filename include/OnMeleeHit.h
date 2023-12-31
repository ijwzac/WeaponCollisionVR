#pragma once

#include <RE/Skyrim.h>
#include "Settings.h"

using namespace SKSE;
using namespace SKSE::log;

// Stores the original OnMeleeHit function. Currently only store one
class OriMeleeHit {
public:
    int64_t shouldHitFrame;
    RE::Actor* hit_causer;
    RE::Actor* hit_target;
    std::int64_t a_int1;
    bool a_bool;
    void* a_unkptr;
    OriMeleeHit()
        : shouldHitFrame(-1),
          hit_causer(nullptr),
          hit_target(nullptr),
          a_int1(0),
          a_bool(false),
          a_unkptr(nullptr) {}

    OriMeleeHit(int64_t shouldHitF, RE::Actor* hit_causer, RE::Actor* hit_target, std::int64_t a_int1, bool a_bool,
                void* a_unkptr)
        : shouldHitFrame(shouldHitF),
          hit_causer(hit_causer),
          hit_target(hit_target),
          a_int1(a_int1),
          a_bool(a_bool),
          a_unkptr(a_unkptr) {}
};

class OriMeleeQueue {
public:
    std::vector<OriMeleeHit> buffer;
    std::size_t capacity;
    std::size_t indexCurrent;

    OriMeleeQueue(std::size_t cap) : buffer(cap), capacity(cap), indexCurrent(0) {}

    void Clear() {
        for (std::size_t i = 0; i < capacity; i++) {
            buffer[i].shouldHitFrame = -1;
        }
    }

    void PushCopy(OriMeleeHit h) { 
        log::trace("In OriMeleeQueue::PushCopy");
        buffer[indexCurrent] = h;
        indexCurrent = (indexCurrent + 1) % capacity;
    }

    // May return null. Caller should call this multiple times until return null
    OriMeleeHit* GetMatchOriMelee(int64_t currentFrame) {
        log::trace("In OriMeleeQueue::GetMatchOriMelee");
        if (buffer.empty()) {
            return nullptr;
        }
        std::size_t i = indexCurrent;
        do {
            i = (i == 0) ? capacity - 1 : i - 1;
            if (buffer[i].shouldHitFrame == -1) continue;
            log::trace("-{}- buffer[{}].shouldHitFrame == {}, caused by {}", currentFrame, i, buffer[i].shouldHitFrame,
                       buffer[i].hit_causer->GetDisplayFullName());
            if (buffer[i].shouldHitFrame == currentFrame) {
                return &buffer[i];
            }
        } while (i != indexCurrent);
        return nullptr;
    }
};

extern OriMeleeQueue meleeQueue;

class DistResult {
public:
    float dist;
    RE::NiPoint3 contactPoint;
    bool proj_isLeft; // Warning: this field is not filled if the parry is not between weapon and projectile

    DistResult(float d, RE::NiPoint3 p) : dist(d), contactPoint(p) {}
    DistResult(float d, RE::NiPoint3 p, bool proj_isLeft) : dist(d), contactPoint(p), proj_isLeft(proj_isLeft) {}
    DistResult() : dist(9999.0f) {}
};

RE::NiPoint3 closestPointOnSegmentForPoint(RE::NiPoint3& segStart, RE::NiPoint3& segEnd, RE::NiPoint3& target);

float distPoint2Segment(RE::NiPoint3& segStart, RE::NiPoint3& segEnd, RE::NiPoint3& target);

RE::NiPoint3 projectPointOntoCirclePlance(RE::NiPoint3& circleCenter, RE::NiPoint3& circleNormal, RE::NiPoint3& target);

DistResult DistForShield(RE::NiPoint3& shieldCenter, RE::NiPoint3& shieldNormal, float radius, RE::NiPoint3& weapP1,
                         RE::NiPoint3& weapP2);

namespace OnMeleeHit {
#pragma warning(push)
#pragma warning(disable : 4251)

    class __declspec(dllexport) OnMeleeHitHook {
    public:
        [[nodiscard]] static OnMeleeHitHook& GetSingleton() noexcept;

        static void InstallHook();


    private:
        OnMeleeHitHook() = default;
        OnMeleeHitHook(const OnMeleeHitHook&) = delete;
        OnMeleeHitHook(OnMeleeHitHook&&) = delete;
        ~OnMeleeHitHook() = default;

        OnMeleeHitHook& operator=(const OnMeleeHitHook&) = delete;
        OnMeleeHitHook& operator=(OnMeleeHitHook&&) = delete;

        /** Our new function */
        static void OnMeleeHit(RE::Actor* attacker, RE::Actor* target, std::int64_t a_int1, bool a_bool,
                               void* a_unkptr);

        /** The original function */
        static inline REL::Relocation<decltype(OnMeleeHit)> _OnMeleeHit;

    public:
        static void FireOriMeleeHit(RE::Actor* attacker, RE::Actor* target, std::int64_t a_int1, bool a_bool,
                                    void* a_unkptr) {
            _OnMeleeHit(attacker, target, a_int1, a_bool, a_unkptr);
        }
    };

    bool IsAttacking(const RE::ATTACK_STATE_ENUM state_a);
    DistResult Dist(const RE::NiPoint3& A, const RE::NiPoint3& B, const RE::NiPoint3& C, const RE::NiPoint3& D);
    RE::NiPoint3 Lerp(const RE::NiPoint3& A, const RE::NiPoint3& B, const float k);
    float Clamp01(const float t);
    RE::NiPoint3 constrainToSegment(const RE::NiPoint3& position, const RE::NiPoint3& a, const RE::NiPoint3& b);
    float dist(const RE::NiPoint3& A, const RE::NiPoint3& B, const RE::NiPoint3& C);
    void play_sound(RE::TESObjectREFR* object, RE::FormID formid);
/*Impacts:
(Default one) 0x0004BB52 WPNBlade1HandVsMetaImpact
 0x0004BB53 WPNBluntVsMetalImpact
 0x0004BB54 WPNArrowVsMetalImpact ; maybe try this out to have Dplay_Block_SparksN.esp increase the effect?
 0x0004BB55 WPNAxeVsMetalImpact
 0x0005CEF7 PHYGenericMetalSmallImpact
 0x0005CEF8 PHYGenericMetalMediumImpact
 0x0005CEFB PHYGenericMetalHeavyImpact
*/
    bool play_impact_1(RE::Actor* actor, const RE::BSFixedString& nodeName);
    bool debug_show_weapon_collide(RE::Actor* actor, const RE::BSFixedString& nodeName);
    bool play_impact_2(RE::TESObjectREFR* a, RE::BGSImpactData* impact, RE::NiPoint3* P_V, RE::NiPoint3* P_from,
                     RE::NiNode* bone);
    bool play_impact_3(RE::TESObjectCELL* cell, float a_lifetime, const char* model, RE::NiPoint3* a_rotation,
                       RE::NiPoint3* a_position, float a_scale, uint32_t a_flags, RE::NiNode* a_target);

    // From: https://github.com/ersh1/Precision/blob/567978e940dc36dc2f68d704fc2ccec4a77a7f10/src/Offsets.h#L334
    typedef float (*tActor_GetReach)(RE::Actor* a_this);
    static REL::Relocation<tActor_GetReach> Actor_GetReach{RELOCATION_ID(37588, 38538)};  // 623F10, 649520

    // From: https://github.com/D7ry/valhallaCombat/blob/2d686d2bddca3b3448af3af3c6b43e2fb3f5ced9/src/include/offsets.h#L48
    inline int soundHelper_a(void* manager, RE::BSSoundHandle* a2, int a3, int a4)  // sub_140BEEE70
    {
        using func_t = decltype(&soundHelper_a);
        REL::Relocation<func_t> func{RELOCATION_ID(66401, 67663)};
        return func(manager, a2, a3, a4);
    }

    // From: https://github.com/D7ry/valhallaCombat/blob/2d686d2bddca3b3448af3af3c6b43e2fb3f5ced9/src/include/offsets.h#L55
    inline void soundHelper_b(RE::BSSoundHandle* a1, RE::NiAVObject* source_node)  // sub_140BEDB10
    {
        using func_t = decltype(&soundHelper_b);
        REL::Relocation<func_t> func{RELOCATION_ID(66375, 67636)};
        return func(a1, source_node);
    }

    // From: https://github.com/D7ry/valhallaCombat/blob/2d686d2bddca3b3448af3af3c6b43e2fb3f5ced9/src/include/offsets.h#L62
    inline char __fastcall soundHelper_c(RE::BSSoundHandle* a1)  // sub_140BED530
    {
        using func_t = decltype(&soundHelper_c);
        REL::Relocation<func_t> func{RELOCATION_ID(66355, 67616)};
        return func(a1);
    }

    // From: https://github.com/D7ry/valhallaCombat/blob/2d686d2bddca3b3448af3af3c6b43e2fb3f5ced9/src/include/offsets.h#L69
    inline char set_sound_position(RE::BSSoundHandle* a1, float x, float y, float z) {
        using func_t = decltype(&set_sound_position);
        REL::Relocation<func_t> func{RELOCATION_ID(66370, 67631)};
        return func(a1, x, y, z);
    }

  

#pragma warning(pop)
}  // namespace OnMeleeHit

