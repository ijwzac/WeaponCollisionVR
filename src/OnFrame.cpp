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
    if (bEnableWholeMod) {
        if (const auto ui{RE::UI::GetSingleton()}) {
            if (!ui->GameIsPaused()) {  // Not in menu, not in load, not in console
                ZacOnFrame::CollisionDetection();
                iFrameCount++;
            } else {
                // log::trace("Game paused, don't record collision");
            }
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

    // See if we need to fire a delayed melee event
    log::trace("About to check meleeQueue");
    auto lastMeleeHit = meleeQueue.GetMatchOriMelee(iFrameCount);
    while (lastMeleeHit) {
        lastMeleeHit->shouldHitFrame = -1;  // this doesn't delete it, but won't be used and will eventually be cleared
        log::trace("Handling a delayed hit");
        if (lastMeleeHit->hit_causer) {
            auto col = colBuffer.GetThisEnemyLatestCollision(lastMeleeHit->hit_causer);
            if (col && !col->shouldNullifyEnemyCurretHit()) {
                log::trace("A delayed hit is fired");
                OnMeleeHit::OnMeleeHitHook::GetSingleton().FireOriMeleeHit(
                    lastMeleeHit->hit_causer, lastMeleeHit->hit_target, lastMeleeHit->a_int1, lastMeleeHit->a_bool,
                    lastMeleeHit->a_unkptr);
            }
        }
        lastMeleeHit = meleeQueue.GetMatchOriMelee(iFrameCount);
    }
    log::trace("Finished checking meleeQueue");

    if (playerActor->IsBlocking()) {
        log::trace("Player is blocking");
        return;
    }

    
    // Get player's weapons
    RE::NiPoint3 posWeaponBottomL, posWeaponBottomR, posWeaponTopL, posWeaponTopR;
    if (!FrameGetWeaponPos(playerActor, posWeaponBottomL, posWeaponBottomR, posWeaponTopL, posWeaponTopR, true)) {
        log::warn("Fail to get player's weapon pos");
        return;
    }
    speedBuf.Push(WeaponPos(posWeaponBottomL, posWeaponTopL), true);
    speedBuf.Push(WeaponPos(posWeaponBottomR, posWeaponTopR), false);

    // If bPlayerMustBeAttacking is true, which is default true for SE/AE, and default false for VR
    // Then we return here, because player must be attacking to parry
    if (bPlayerMustBeAttacking) {
        if (!playerActor->AsActorState()) {
            log::warn("Fail to get ActorState for player");
            return;
        }
        if (!OnMeleeHit::IsAttacking(playerActor->AsActorState()->GetAttackState())) {
            log::trace("Player not attacking");
            return;
        }
    }


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

            // See if a recent collision happens to this actor
            Collision* recentCol = colBuffer.GetThisEnemyLatestCollision(actorNPC);
            if (recentCol) {
                if ((*recentCol).shouldIgnoreCollision()) {
                    logger::trace("Ignore collision and push enemy. Enemy:{}", actorNPC->GetBaseObject()->GetName());
                    recentCol->ChangeVelocity();
                    recentCol->ChangeAngle();
                    continue;
                }
            }

            float angleZ2 = 0.0f;
            if (auto enemyRef = static_cast<RE::TESObjectREFR*>(actorNPC); enemyRef) {
                angleZ2 = enemyRef->GetAngleZ();
                log::trace("Enemy AngleZ: {}", angleZ2);
            }
            // TODO: Check if NPC is enemy
            

            // TODO: consider make it that even if NPC is not attacking, still enable collision

            // TODO: how to see if NPC is using both two weapons? Flags kRotatingAttack isn't for this
            //      Also, 44313 has an attack move that scissors the player with two hands, it's right hand and no flag can detect it
            

            // Check if NPC is attacking, which hand.
            bool isLeftAttack;
            bool isPowerAttack;
            if (!actorNPC->AsActorState()) {
                log::warn("Fail to get ActorState for NPC");
                continue;
            }
            if (!OnMeleeHit::IsAttacking(actorNPC->AsActorState()->GetAttackState())) {
                log::trace("NPC not attacking");
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
            isLeftAttack = attackerAI->high->attackData->IsLeftAttack();
            isPowerAttack = attackerAI->high->attackData->data.flags.any(RE::AttackData::AttackFlag::kPowerAttack);

            // Get enemies' weapons
            RE::NiPoint3 posNPCWeaponBottomL, posNPCWeaponBottomR, posNPCWeaponTopL, posNPCWeaponTopR;
            if (!FrameGetWeaponPos(actorNPC, posNPCWeaponBottomL, posNPCWeaponBottomR, posNPCWeaponTopL,
                                   posNPCWeaponTopR, false)) {
                log::warn("Fail to get NPC's weapon pos");
                continue;
            }

            // See if player's weapon collides with enemies' weapons
            // For now, we check both the collision between any weapon regardlessly 
            //      (enemy's L and player's L and R, and enemy's R and player's L and R)

            RE::NiPoint3 contactPos; // approximately where collision happens
            auto dis_playerL_enemyL =
                OnMeleeHit::Dist(posWeaponBottomL, posWeaponTopL, posNPCWeaponBottomL, posNPCWeaponTopL);
            auto dis_playerL_enemyR =
                OnMeleeHit::Dist(posWeaponBottomL, posWeaponTopL, posNPCWeaponBottomR, posNPCWeaponTopR);
            auto dis_playerR_enemyL =
                OnMeleeHit::Dist(posWeaponBottomR, posWeaponTopR, posNPCWeaponBottomL, posNPCWeaponTopL);
            auto dis_playerR_enemyR =
                OnMeleeHit::Dist(posWeaponBottomR, posWeaponTopR, posNPCWeaponBottomR, posNPCWeaponTopR);

            bool isCollision = false;
            bool isEnemyLeft = false;
            bool isPlayerLeft = false;
            ;
            if (dis_playerL_enemyL.dist < fCollisionDistThres) {
                isCollision = true;
                isEnemyLeft = true;
                isPlayerLeft = true;
                contactPos = dis_playerL_enemyL.contactPoint;
            } else if (dis_playerL_enemyR.dist < fCollisionDistThres) {
                isCollision = true;
                isPlayerLeft = true;
                contactPos = dis_playerL_enemyR.contactPoint;
            } else if (dis_playerR_enemyL.dist < fCollisionDistThres) {
                isCollision = true;
                isEnemyLeft = true;
                contactPos = dis_playerR_enemyL.contactPoint;
            } else if (dis_playerR_enemyR.dist < fCollisionDistThres) {
                isCollision = true;
                contactPos = dis_playerR_enemyR.contactPoint;
            } 
            if (isCollision) {
                // create a new collision

                // push the enemy away by this velocity. Will be used for the next few frames
                RE::hkVector4 pushVelocity =
                    CalculatePushVector(playerActor->GetPosition(), actorNPC->GetPosition(), true);

                // get the angle of enemy. Will be used for the next few frames
                float angleZ = 0.0f;
                if (auto enemyRef = static_cast<RE::TESObjectREFR*>(actorNPC); enemyRef) {
                    angleZ = enemyRef->GetAngleZ();
                }
                RE::NiPoint3 playerWeapSpeed = speedBuf.GetVelocity(5, isPlayerLeft);
                bool isRotClockwise = ShouldRotateClockwise(playerActor->GetPosition(), actorNPC->GetPosition(), playerWeapSpeed);
                int64_t rotDurationFrame = RotateFrame(playerWeapSpeed.SqrLength());

                auto col = Collision(actorNPC, iFrameCount, actorNPC->GetPosition(), pushVelocity, angleZ,
                                     isRotClockwise, rotDurationFrame);
                log::trace("Collision with {} on frame {}", col.getEnemy()->GetBaseObject()->GetName(),
                           col.getFrame());
                colBuffer.PushCopy(col);

                // Also update iFrameLastCollision, used to prevent player's hit
                iFrameLastCollision = iFrameCount;

                // Collision effects
                CollisionEffect(playerActor, actorNPC, contactPos, isEnemyLeft, isPlayerLeft);
            }
        }
    }

    

    

    // Create collision data
}

void ZacOnFrame::CollisionEffect(RE::Actor* playerActor, RE::Actor* enemyActor, RE::NiPoint3 contactPos, bool isEnemyLeft, bool isPlayerLeft) {
    const auto nodeName = isEnemyLeft?
        "SHIELD"sv: "WEAPON"sv;
    auto root = netimmerse_cast<RE::BSFadeNode*>(enemyActor->Get3D());
    if (!root) return;
    auto bone = netimmerse_cast<RE::NiNode*>(root->GetObjectByName(nodeName));
    if (!bone) return;
    const auto nodeNameFoot = "NPC L Toe0 [LToe]"sv;
    auto boneFoot = netimmerse_cast<RE::NiNode*>(root->GetObjectByName(nodeNameFoot));
    // Display spark and sound
    SKSE::GetTaskInterface()->AddTask([enemyActor, contactPos, bone, boneFoot]() {
        RE::NiPoint3 contact = contactPos;
        RE::NiPoint3 P_V = {0.0f, 0.0f, 0.0f};
        OnMeleeHit::play_sound(enemyActor, 0x0003C73C);
        // Display spark on enemy's weapon, at the collision point
        OnMeleeHit::play_impact_2(enemyActor, RE::TESForm::LookupByID<RE::BGSImpactData>(0x0004BB54), &P_V, &contact,
                                  bone);

        if (!boneFoot) return;
        // Display dust under enemy's foot
        RE::NiPoint3 foot = boneFoot->world.translate;
        // 13CBA, WPNBlade1HandVsDirtImpact; 
        OnMeleeHit::play_impact_2(enemyActor, RE::TESForm::LookupByID<RE::BGSImpactData>(0x00013CBA), &P_V, &foot,
                                  boneFoot);
    });

    // if enemy is power attacking, double sta cost to player
    bool isEnemyPower = false;
    if (auto enemyAI = enemyActor->GetActorRuntimeData().currentProcess; enemyAI) {
        if (enemyAI->high && enemyAI->high->attackData &&
            enemyAI->high->attackData->data.flags.any(RE::AttackData::AttackFlag::kPowerAttack)) {
            isEnemyPower = true;
            log::trace("Enemy is power attacking!");
        }
    }
    // if player is power attacking, double sta cost to player
    bool isPlayerPower = false;
    if (auto playerAI = playerActor->GetActorRuntimeData().currentProcess; playerAI) {
        if (playerAI->high && playerAI->high->attackData &&
            playerAI->high->attackData->data.flags.any(RE::AttackData::AttackFlag::kPowerAttack)) {
            isPlayerPower = true;
            log::trace("Player is power attacking!");
        }
    }

    // enemy stamina cost and may recoil
    if (!enemyActor->AsActorValueOwner()) {
        log::error("Enemy doesn't have actorvalueowner:{}", enemyActor->GetDisplayFullName());
        return;
    } else if (!playerActor->AsActorValueOwner()) {
        log::error("Player doesn't have actorvalueowner:{}", playerActor->GetDisplayFullName());
        return;
    } 
    auto enemyCurSta = enemyActor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
    uint16_t playerDamage(5), enemyDamage(5); // 5 for unhanded
    if (playerActor->GetEquippedObject(isPlayerLeft)) {
        if (auto weap = playerActor->GetEquippedObject(isPlayerLeft)->As<RE::TESObjectWEAP>(); weap) {
            playerDamage = weap->attackDamage > playerDamage ? weap->attackDamage: playerDamage;
            auto oriDmg = playerDamage;
            if (IsOneHandWeap(playerActor, isPlayerLeft))
                playerDamage =
                    (uint16_t)((float)playerDamage *
                               (1.0f + playerActor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kOneHanded) / 100.0f));
            else if (IsTwoHandWeap(playerActor, isPlayerLeft))
                playerDamage =
                    (uint16_t)((float)playerDamage *
                               (1.0f +
                                playerActor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kTwoHanded) / 100.0f));
            else if (bHandToHandLoad && IsHandToHand(playerActor, isPlayerLeft))
                playerDamage =
                    (uint16_t)((float)playerDamage *
                               (1.0f +
                                playerActor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kLockpicking) / 100.0f));
            log::trace("Player damage: {}. Ori damage: {}", playerDamage, oriDmg);
        }
    }
    if (enemyActor->GetEquippedObject(isEnemyLeft)) {
        if (auto weap = enemyActor->GetEquippedObject(isEnemyLeft)->As<RE::TESObjectWEAP>(); weap) {
            enemyDamage = weap->attackDamage > enemyDamage ? weap->attackDamage : enemyDamage;
            auto oriDmg = enemyDamage;
            if (IsOneHandWeap(enemyActor, isEnemyLeft))
                enemyDamage =
                    (uint16_t)((float)enemyDamage *
                               (1.0f +
                                enemyActor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kOneHanded) / 100.0f));
            else if (IsTwoHandWeap(enemyActor, isEnemyLeft))
                enemyDamage =
                    (uint16_t)((float)enemyDamage *
                               (1.0f +
                                enemyActor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kTwoHanded) / 100.0f));
            log::trace("Enemy damage: {}. Ori damage: {}", enemyDamage, oriDmg);
        }
    }
    float enemyStaCost = fEnemyStaCostWeapMulti * (float) ( 1.5 * playerDamage - enemyDamage);
    if (isPlayerPower && !isEnemyPower) {
        enemyStaCost *= 2;
    }
    enemyStaCost = enemyStaCost < fEnemyStaCostMin ? fEnemyStaCostMin : enemyStaCost;
    enemyStaCost = enemyStaCost > fEnemyStaCostMax ? fEnemyStaCostMax : enemyStaCost;
    float enemyFinalSta = enemyCurSta - enemyStaCost;
    if (enemyFinalSta < 0.0f) {
        enemyFinalSta = 0.0f;
    }
    enemyStaCost = enemyCurSta - enemyFinalSta;
    enemyActor->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina,
                                                       -enemyStaCost);

    log::trace("Player damage: {}. Enemy cur sta:{}. cost:{}. final:{}", playerDamage, enemyCurSta,
               enemyStaCost, enemyFinalSta);
    log::trace("Enemy perm sta:{}", enemyActor->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kStamina));
    // Enemy recoil
    if (isPlayerPower || enemyFinalSta <
        fEnemyStaLargeRecoilThresPer *
                            enemyActor->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kStamina)) {
         enemyActor->NotifyAnimationGraph("recoilStop");
         enemyActor->NotifyAnimationGraph("AttackStop");
         enemyActor->NotifyAnimationGraph("recoilLargeStart");
    } else if (enemyFinalSta < fEnemyStaStopThresPer *
                                   enemyActor->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kStamina)) {
            enemyActor->NotifyAnimationGraph("recoilStop");
            enemyActor->NotifyAnimationGraph("AttackStop");
    }

    // Player stamina cost
    auto playerCurSta = playerActor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
    float playerStaCost = fPlayerStaCostWeapMulti * (float)(1.5 * enemyDamage - playerDamage);
    if (isEnemyPower && !isPlayerPower) playerStaCost *= 2;
    playerStaCost = playerStaCost < fPlayerStaCostMin ? fPlayerStaCostMin : playerStaCost;
    playerStaCost = playerStaCost > fPlayerStaCostMax ? fPlayerStaCostMax : playerStaCost;
    float playerFinalSta = playerCurSta - playerStaCost;
    if (playerFinalSta < 0.0f) {
            playerFinalSta = 0.0f;
    }
    playerStaCost = playerCurSta - playerFinalSta;
    // if player is moving their weapon, reduce stamina cost
    auto speed = speedBuf.GetVelocity(5, isPlayerLeft).SqrLength();
    log::trace("Player moving their weapon at speed:{}, higher than thres1:{}, higher than thres2:{}", speed,
               speed > fPlayerWeaponSpeedRewardThres, speed > fPlayerWeaponSpeedRewardThres2);
    if (speed > fPlayerWeaponSpeedRewardThres) {
        playerStaCost *= fPlayerWeaponSpeedReward;
    } else if (speed > fPlayerWeaponSpeedRewardThres2) {
            playerStaCost *= fPlayerWeaponSpeedReward2;
    } 
    playerFinalSta = playerCurSta - playerStaCost;
    playerActor->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina,
                                                        -playerStaCost);
    log::trace("Enemy damage: {}. player cur sta:{}. cost:{}. final:{}", enemyDamage, playerCurSta, playerStaCost,
               playerFinalSta);

    // Player recoil
    if (playerFinalSta < fPlayerStaLargeRecoilThresPer *
                              playerActor->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kStamina)) {
            playerActor->NotifyAnimationGraph("recoilStop");
            playerActor->NotifyAnimationGraph("AttackStop");
            playerActor->NotifyAnimationGraph("recoilLargeStart");
    } else if (playerFinalSta < fPlayerStaStopThresPer * playerActor->AsActorValueOwner()->GetPermanentActorValue(
                                                             RE::ActorValue::kStamina)) {
            playerActor->NotifyAnimationGraph("recoilStop");
            playerActor->NotifyAnimationGraph("AttackStop");
    }

    // Haptic vibration on controller, based on playerStaCost
    auto papyrusVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (papyrusVM) {
        int hapticFrame = (int)(playerStaCost * fHapticMulti);
        hapticFrame = hapticFrame < iHapticStrMin ? iHapticStrMin : hapticFrame;
        hapticFrame = hapticFrame > iHapticStrMax ? iHapticStrMax : hapticFrame;

        log::trace("Preparing arguments");
        RE::BSScript::IFunctionArguments* hapticArgs;
        if (isPlayerLeft) {
            hapticArgs = RE::MakeFunctionArguments(true, (int)hapticFrame, (int)iHapticLengthMicroSec);
        } else {
            hapticArgs = RE::MakeFunctionArguments(false, (int)hapticFrame, (int)iHapticLengthMicroSec);
        }
        
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
       
        if (papyrusVM->TypeIsValid("VRIK"sv)) {
            log::trace("Calling papyrus");
            // Function VrikHapticPulse(Bool onLeftHand, Int frames, Int microsec) native global
            papyrusVM->DispatchStaticCall("VRIK"sv, "VrikHapticPulse"sv, hapticArgs, callback);
            log::trace("Finished calling papyrus");
        } else {
            log::trace("VRIK not installed");
        }
    } 
        
    

    // adv block and weapon skill
    auto playerAA = RE::PlayerCharacter::GetSingleton();
    if (playerAA) {
        playerAA->AddSkillExperience(RE::ActorValue::kBlock, fExpBlock);
        if (IsOneHandWeap(playerActor, isPlayerLeft))
            playerAA->AddSkillExperience(RE::ActorValue::kOneHanded, fExpOneHand);
        else if (IsTwoHandWeap(playerActor, isPlayerLeft))
            playerAA->AddSkillExperience(RE::ActorValue::kTwoHanded, fExpTwoHand);
        else if (bHandToHandLoad && IsHandToHand(playerActor, isPlayerLeft))
            playerAA->AddSkillExperience(RE::ActorValue::kLockpicking, fExpHandToHand);
    }
}


void debug_show_weapon_range(RE::Actor* actor, RE::NiPoint3& posWeaponBottom, RE::NiPoint3& posWeaponTop,
                             RE::NiNode* bone) {
    RE::NiPoint3 P_V = {0.0f, 0.0f, 0.0f};
    OnMeleeHit::play_impact_2(actor, RE::TESForm::LookupByID<RE::BGSImpactData>(0x0004BB54), &P_V, &posWeaponTop, bone);
    OnMeleeHit::play_impact_2(actor, RE::TESForm::LookupByID<RE::BGSImpactData>(0x0004BB54), &P_V, &posWeaponBottom, bone);
}


// This function modifies the 4 NiPoint3 passed in. If failed to get the 3D of actor, it return false.
// Otherwise, it returns true, even if some NiPoint3 are not modified. 
// Caller needs to detect if they are modified, by calling IsNiPointZero()
bool ZacOnFrame::FrameGetWeaponPos(RE::Actor* actor, RE::NiPoint3& posWeaponBottomL, RE::NiPoint3& posWeaponBottomR,
                       RE::NiPoint3& posWeaponTopL,
                       RE::NiPoint3& posWeaponTopR, bool isPlayer) {
    const auto actorRoot = netimmerse_cast<RE::BSFadeNode*>(actor->Get3D());
    if (!actorRoot) {
        log::warn("Fail to find actor:{}", actor->GetBaseObject()->GetName());
        return false;
    }

    const auto nodeNameL =
        "SHIELD"sv;  // This is the node name of left hand weapon, no matter if it's shield, sword, unarmed
    const auto nodeNameR = "WEAPON"sv;

    const auto weaponL = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(nodeNameL));
    const auto weaponR = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(nodeNameR));


    // check whether the actor really equips a weapon on that hand
    // one-hand weapon: correctly on each hand
    // fist: equipL/R is null
    // bow: both hand are same
    // two-hand weapon: both hands are the same
    // shield: on left hand
    bool isEquipL(false), isEquipR(false), isBow(false), isTwoHandedAxe(false), isWarHammer(false),
        isGreatSword(false), hasShield(false), isSwordL(false), isSwordR(false), isAxeL(false), isAxeR(false),
        isMaceL(false), isMaceR(false), isStaffL(false), isStaffR(false), isFistL(false), isFistR(false),
        isDaggerL(false), isDaggerR(false);
    if (auto equipR = actor->GetEquippedObject(false); equipR) {
        isEquipR = true;
        if (auto equipWeapR = equipR->As<RE::TESObjectWEAP>(); equipWeapR) {
            if (equipWeapR->HasKeywordString("WeapTypeDagger")) {
                isDaggerR = true;
            } else if (equipWeapR->HasKeywordString("WeapTypeMace")) {
                isMaceR = true;
            } else if (equipWeapR->HasKeywordString("WeapTypeSword")) {
                isSwordR = true;
            } else if (equipWeapR->HasKeywordString("WeapTypeWarAxe")) {
                isAxeR = true;
            } else if (equipWeapR->IsStaff()) {
                isStaffR = true;
            }
        }
    } else {
        // fist
        isFistR = true;
    }
    if (auto equipL = actor->GetEquippedObject(true); equipL) {
        isEquipL = true;
        if (auto equipWeapL = equipL->As<RE::TESObjectWEAP>(); equipWeapL) {
            if (equipWeapL->IsBow()) {
                isBow = true;
                isEquipL = true;
                isEquipR = false;
            } else if (equipWeapL->IsTwoHandedSword()) {
                isGreatSword = true;
                isEquipL = false;
                isEquipR = true;
            } else if (equipWeapL->IsTwoHandedAxe() ) {
                isTwoHandedAxe = true;
                isEquipL = false;
                isEquipR = true;
            } else if (equipWeapL->HasKeywordString("WeapTypeWarhammer")) {
                isWarHammer = true;
                isEquipL = false;
                isEquipR = true;
            } else if (equipWeapL->HasKeywordString("WeapTypeDagger")) {
                isDaggerL = true;
            } else if (equipWeapL->HasKeywordString("WeapTypeMace")) {
                isMaceL = true;
            } else if (equipWeapL->HasKeywordString("WeapTypeSword")) {
                isSwordL = true;
            } else if (equipWeapL->HasKeywordString("WeapTypeWarAxe")) {
                isAxeL = true;
            } else if (equipWeapL->IsStaff()) {
                isStaffL = true;
            }
        }
        if (equipL->IsArmor()) {
            hasShield = true;
        }
    } else {
        // fist
        isFistL = true;
    }

    if (weaponL && (isEquipL || (isFistL && isFistR))) { // only enable fist collision when both hands are fist
        float reachL(0.0f), handleL(0.0f);
        if (isFistL) { 
            reachL = 10.0f;
            handleL = 10.0f;
        } else if (isBow) { // OK
            reachL = 70.0f;
            handleL = 70.0f;
        } else if (isDaggerL) { // OK
            reachL = 20.0f;
            handleL = 10.0f;
        } else if (isSwordL) { // OK
            reachL = 90.0f;
            handleL = 12.0f;
        } else if (isAxeL) { // OK
            reachL = 60.0f;
            handleL = 12.0f;
        } else if (isMaceL) { // OK
            reachL = 60.0f;
            handleL = 12.0f;
        } else if (isStaffL) { // OK
            reachL = 70.0f;
            handleL = 70.0f;
        } else if (hasShield) {
            reachL = 30.0f;
            handleL = 30.0f;
        }
        reachL *= fRangeMulti;
        handleL *= fRangeMulti;
        posWeaponBottomL = weaponL->world.translate;
        const auto weaponDirectionL = RE::NiPoint3{weaponL->world.rotate.entry[0][1], weaponL->world.rotate.entry[1][1],
                                                   weaponL->world.rotate.entry[2][1]};
        posWeaponTopL = posWeaponBottomL + weaponDirectionL * reachL;
        posWeaponBottomL = posWeaponBottomL - weaponDirectionL * handleL;
        
        if (bShowWeaponSegment) debug_show_weapon_range(actor, posWeaponBottomL, posWeaponTopL, weaponL);

    } else {
        // Animals, dragons will reach here
        log::trace("Doesn't get left weapon. actor:{}", actor->GetBaseObject()->GetName());
        /*if (isPlayer) {
            SetNiPointSpecial(posWeaponBottomL);
            SetNiPointSpecial(posWeaponTopL);
        }*/
        
    }
    
    if (weaponR && (isEquipR || (isFistL && isFistR))) {
        float reachR(0.0f), handleR(0.0f);
        if (isFistR) {
            reachR = 10.0f;
            handleR = 10.0f;
        }else if (isTwoHandedAxe || isWarHammer) { // OK
            reachR = 70.0f;
            handleR = 70.0f;
        } else if (isGreatSword) { // OK
            reachR = 100.0f;
            handleR = 20.0f;
        } else if (isDaggerR) { // OK
            reachR = 20.0f;
            handleR = 10.0f;
        } else if (isSwordR) {  // OK
            reachR = 90.0f;
            handleR = 12.0f;
        } else if (isAxeR) {  // OK
            reachR = 60.0f;
            handleR = 12.0f;
        } else if (isMaceR) {  // OK
            reachR = 60.0f;
            handleR = 12.0f;
        } else if (isStaffR) {  // OK
            reachR = 70.0f;
            handleR = 70.0f;
        }
        reachR *= fRangeMulti;
        handleR *= fRangeMulti;
        posWeaponBottomR = weaponR->world.translate;
        const auto weaponDirectionR = RE::NiPoint3{weaponR->world.rotate.entry[0][1], weaponR->world.rotate.entry[1][1],
                                                   weaponR->world.rotate.entry[2][1]};
        posWeaponTopR = posWeaponBottomR + weaponDirectionR * reachR;
        posWeaponBottomR = posWeaponBottomR - weaponDirectionR * handleR;

        if (bShowWeaponSegment) debug_show_weapon_range(actor, posWeaponBottomR, posWeaponTopR, weaponR);
    } else {
        log::trace("Doesn't get right weapon. actor:{}", actor->GetBaseObject()->GetName());
        /*if (isPlayer) {
            SetNiPointSpecial(posWeaponBottomL);
            SetNiPointSpecial(posWeaponTopL);
        }*/
    }

    
    return true;
}


RE::hkVector4 ZacOnFrame::CalculatePushVector(RE::NiPoint3 sourcePos, RE::NiPoint3 targetPos, bool isEnemy) {
    RE::NiPoint3 pushDirection = targetPos - sourcePos;
    // Normalize the direction
    float length =
        sqrt(pushDirection.x * pushDirection.x + pushDirection.y * pushDirection.y);
    if (length > 0.0f) {  // avoid division by zero
        pushDirection.x /= length;
        pushDirection.y /= length;
    }
    float pushMulti = isEnemy ? fEnemyPushMulti : 0.0; // TODO: change this after implement player push
    return RE::hkVector4(pushDirection.x * pushMulti, pushDirection.y * pushMulti, 0.0f, 0.0f);
}