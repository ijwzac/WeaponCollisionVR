#include "Utils.h"
#include <RE/Skyrim.h>
#include "OnFrame.h"
#include "Settings.h"
#include "OnMeleeHit.h"

using namespace SKSE;
using namespace SKSE::log;

using namespace ZacOnFrame;

// The hook below is taught by ThirdEyeSqueegee and Flip on Discord. Related code:
// https://github.com/ThirdEyeSqueegee/DynamicGamma/blob/main/include/GammaController.h#L16 and
// https://github.com/doodlum/MaxsuWeaponSwingParry-ng/blob/main/src/WeaponParry_Hooks.h
// TODO: Some numbers are magic numbers from their code. Figure them out
void ZacOnFrame::InstallFrameHook() {
    SKSE::AllocTrampoline(1 << 4);
    auto& trampoline = SKSE::GetTrampoline();

    REL::Relocation<std::uintptr_t> OnFrameBase{REL::RelocationID(35565, 36564)};
    _OnFrame =
        trampoline.write_call<5>(OnFrameBase.address() + REL::Relocate(0x748, 0xc26, 0x7ee), ZacOnFrame::OnFrameUpdate);
}

void ZacOnFrame::OnFrameUpdate() {
    if (const auto ui{RE::UI::GetSingleton()}) {
        if (!ui->GameIsPaused()) { // Not in menu, not in load, not in console
            ZacOnFrame::CollisionDetection();
            iFrameCount++;
        } else {
            //log::trace("Game paused, don't record collision");
        }
    }
    
    ZacOnFrame::_OnFrame();  // TODO: figure out why the game works fine without this line
                 // Anyway, we should always call it
}

void ZacOnFrame::CollisionDetection() {
    // log::trace("iFrameCount is:{}", iFrameCount);
    // Get player
    auto playerAA = RE::PlayerCharacter::GetSingleton();
    if (!playerAA) {
        log::warn("Can't get player!");
        return;
    }

    auto playerActor = static_cast<RE::Actor*>(playerAA);
    auto playerRef = static_cast<RE::TESObjectREFR*>(playerAA);
    if (!playerActor) {
        log::warn("Fail to cast player to Actor");
        return;
    } else if (!playerRef) {
        log::warn("Fail to cast player to ObjRef");
        return;
    }
    if (!playerActor->IsInCombat()) { // Note: Combat state is not related to whether player sheathe weapons
        log::trace("Player not in combat");
        return;
    }

    if (!playerActor->Is3DLoaded()) {
        log::warn("Player not 3D loaded. Shouldn't happen");
        return;
    }
    const auto playerRoot = netimmerse_cast<RE::BSFadeNode*>(playerActor->Get3D());
    if (!playerRoot) {
        log::warn("Fail to find playerRoot");
        return;
    }
    
    // Get player's weapons
    const auto nodeNameL = "SHIELD"sv; // This is the node name of left hand weapon, no matter if it's shield, sword, unarmed
    const auto nodeNameR = "WEAPON"sv;

    const auto weaponL = netimmerse_cast<RE::NiNode*>(playerRoot->GetObjectByName(nodeNameL));
    const auto weaponR = netimmerse_cast<RE::NiNode*>(playerRoot->GetObjectByName(nodeNameR));
    if (!weaponL) {
        log::warn("Fail to get player's left weapon");
        return;
    }
    else if (!weaponR) {
        log::warn("Fail to get player's right weapon");
        return;
    }

    const RE::NiPoint3& posWeaponBottomL = weaponL->world.translate;
    const RE::NiPoint3& posWeaponBottomR = weaponR->world.translate;

    const float reach = OnMeleeHit::Actor_GetReach(playerActor) * fRangeMulti;
    const auto weaponDirectionL = RE::NiPoint3{weaponL->world.rotate.entry[0][1], weaponL->world.rotate.entry[1][1],
                                                  weaponL->world.rotate.entry[2][1]};
    const auto weaponDirectionR = RE::NiPoint3{weaponR->world.rotate.entry[0][1], weaponR->world.rotate.entry[1][1],
                                               weaponR->world.rotate.entry[2][1]};
    const RE::NiPoint3& posWeaponTopL = posWeaponBottomL + weaponDirectionL * reach;
    const RE::NiPoint3& posWeaponTopR = posWeaponBottomR + weaponDirectionR * reach;
    // log::trace("posWeaponTopL({},{},{}), reach:{}", posWeaponTopL.x, posWeaponTopL.y, posWeaponTopL.z, reach);

    // Get nearby enemies
    std::vector<RE::TESObjectREFR*> vNearbyObj;
    if (const auto TES = RE::TES::GetSingleton(); TES) {
        TES->ForEachReferenceInRange(playerRef, fDetectEnemy, [&](RE::TESObjectREFR& b_ref) {
            if (const auto base = b_ref.GetBaseObject(); base && b_ref.Is3DLoaded()) {
                if (b_ref.Is(RE::FormType::ActorCharacter)) {
                    vNearbyObj.push_back(&b_ref);
                }
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });
    } else {
        log::warn("Fail to get TES singleton");
        return;
    }
    for (auto* obj : vNearbyObj) {
        if (auto actorNPC = static_cast<RE::Actor*>(obj); actorNPC) {
            if (actorNPC == playerActor || actorNPC->IsDead()) {
                continue;
            }
            logger::trace("Cast success and not player or dead. Name:{}", actorNPC->GetBaseObject()->GetName());

            // TODO: Check if NPC is enemy
            

            // Check if NPC is attacking, which hand.
            // TODO: how to see if NPC is using both two weapons? Flags kRotatingAttack isn't for this
            //      Also, 44313 has an attack move that scissors the player with two hands, it's right hand and no flag can detect it
            bool isLeftAttack;
            bool isPowerAttack;
            if (!actorNPC->AsActorState()) {
                log::warn("Fail to get ActorState");
                continue;
            }
            if (!OnMeleeHit::IsAttacking(actorNPC->AsActorState()->GetAttackState())) {
                //log::trace("NPC not attacking");
                continue;
            }
            RE::AIProcess* const attackerAI = actorNPC->GetActorRuntimeData().currentProcess;
            if (!attackerAI || !attackerAI->high || !attackerAI->high->attackData) {
                log::warn("Fail to get AI, high, or attackData");
                continue;
            }
            if (attackerAI->high->attackData->data.flags.any(RE::AttackData::AttackFlag::kBashAttack)) {
                log::trace("NPC bash attack. Can't parry");
                continue;
            }
            // TODO: handle dual wield triple attack.
            isLeftAttack = attackerAI->high->attackData->IsLeftAttack();
            isPowerAttack = attackerAI->high->attackData->data.flags.any(RE::AttackData::AttackFlag::kPowerAttack);


            const auto NPCRoot = netimmerse_cast<RE::BSFadeNode*>(actorNPC->Get3D());
            if (!NPCRoot) {
                logger::warn("Failed to get NPC Root");
                continue;
            }

            // Immediate TODO: put the get weapon location code for player into a function, and use it here

        }
    }

    // Get enemies' weapons

    // See if player's weapon collides with enemies' weapons

    // Create collision data
}