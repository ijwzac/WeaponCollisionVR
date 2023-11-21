#include "Utils.h"
#include <RE/Skyrim.h>
#include "OnFrame.h"
#include "Settings.h"
#include "OnMeleeHit.h"
#include <chrono>

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

bool isOurFnRunning = false;
long long highest_run_time = 0;
long long total_run_time = 0;
long long run_time_count = 1;

int64_t count_after_pause;
bool bDisableSpark = false;

// I am trying a not very normal approach here: fire the original event first
void ZacOnFrame::OnFrameUpdate() {

    if (isOurFnRunning) {
        log::warn("Our functions are running in parallel!!!"); // Not seen even when stress testing. Keep observing
    }
    isOurFnRunning = true;
    // Get the current time point
    auto now = std::chrono::high_resolution_clock::now();
    bool isPaused = true;
    if (bEnableWholeMod) {
        if (const auto ui{RE::UI::GetSingleton()}) {
            // Check if it has been a while since the last_time, and if so, don't do anything for a few seconds
            // This can disable our mod after loading screen, to avoid bugs
            auto dur_last = std::chrono::duration_cast<std::chrono::microseconds>(now - last_time);
            last_time = now;
            if (dur_last.count() > 1000 * 1000) { // 1 second
                count_after_pause = 180;
                CleanBeforeLoad();
                log::info("Detected long period since last frame. Player is probably loading. Clean and pause our functions for 3 seconds");
            }
            if (count_after_pause > 0) count_after_pause--;

            if (!ui->GameIsPaused() && count_after_pause <= 0) {  // Not in menu, not in console, but can't detect in load

                ZacOnFrame::CollisionDetection();

                iFrameCount++;
                isPaused = false;
            }
        }
    }
    isOurFnRunning = false;
    if (!isPaused) {
        // Convert time point to since epoch (duration)
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - now);

        // Display the microseconds since epoch
        if (highest_run_time < duration.count()) highest_run_time = duration.count();
        total_run_time += duration.count();
        auto average_run_time = total_run_time / run_time_count++;
        if (run_time_count % 1000 == 1) {
            log::info("Exe time of our fn:{} us. Highest:{}. Average:{}. Total:{}. Count:{}", duration.count(), highest_run_time,
                      average_run_time, total_run_time, run_time_count);
        }

        // Tested on 0.9.0 version. Normally takes around 30us when in combat. 0us when not. Highest: 240us
        //                  Stress test with 20 enemies: 60us, highest:1500us.
        //                  Turning trace on slows down average speed by around 2X
        //                  Debug version is like 10X slower, but highest doesn't change much
    }

    ZacOnFrame::_OnFrame();  // TODO: figure out why the game works fine without this line
                             // Anyway, we should always call it
                             // This function takes 0 us. Very fast
                                // TODO: see if we place it here, can the spider save crash again?
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

    if (!playerActor->Is3DLoaded()) {
        log::warn("Player not 3D loaded. Shouldn't happen");
        return;
    }

    if (slowTimeData.shouldRemove(iFrameCount)) {
        StopTimeSlowEffect(playerActor);
    }

    // Check if we should stop blocking
    if (iFrameCount >= iFrameStopBlock && iFrameStopBlock != 0) {
        iFrameStopBlock = 0;
        playerActor->NotifyAnimationGraph("blockStop"sv);
        //// Change block config
        //auto iniHandler = RE::INISettingCollection::GetSingleton();
        //if (iniHandler) {
        //    auto blockEnter = iniHandler->GetSetting("fVRShieldBlockEnterAngle");
        //    auto blockExit = iniHandler->GetSetting("fVRShieldBlockExitAngle");
        //    if (blockEnter && blockExit) {
        //        blockExit->data.f = fOriginExitAngle;
        //        iniHandler->WriteSetting(blockExit);
        //    }

        //} else {
        //    log::warn("iniHandler is null!");
        //}


    }


    // See if we need to fire a delayed melee event
    // log::trace("About to check meleeQueue");
    auto lastMeleeHit = meleeQueue.GetMatchOriMelee(iFrameCount);
    while (lastMeleeHit) {
        lastMeleeHit->shouldHitFrame = -1;  // this doesn't delete it, but won't be used and will eventually be cleared
        log::trace("Handling a delayed hit");
        if (lastMeleeHit->hit_causer) {
            auto col = colBuffer.GetThisEnemyLatestCollision(lastMeleeHit->hit_causer);
            if ((col && !col->shouldNullifyEnemyCurretHit()) || col == nullptr) {
                log::trace("A delayed hit is fired");
                OnMeleeHit::OnMeleeHitHook::GetSingleton().FireOriMeleeHit(
                    lastMeleeHit->hit_causer, lastMeleeHit->hit_target, lastMeleeHit->a_int1, lastMeleeHit->a_bool,
                    lastMeleeHit->a_unkptr);
            }
        }
        lastMeleeHit = meleeQueue.GetMatchOriMelee(iFrameCount);
    }
    // log::trace("Finished checking meleeQueue");

    // Spawn spark on player's weapon and push player away. These functions check frame count internally, so they only act if there is a recent collision
    auto playerCol = PlayerCollision::GetSingleton();
    if (!playerCol->IsEmpty()) {
        playerCol->SpawnSpark();
        playerCol->ChangeVelocity();
    }

    
    if (!playerActor->IsInCombat()) {  // Note: Combat state is not related to whether player sheathe weapons
        log::trace("Player not in combat");
        return;
    }
    
    // Get player's weapons
    bool isPlayerUsingClaw = false;
    if (GetSpecialRace(playerActor) == 3) isPlayerUsingClaw = true;
    RE::NiPoint3 posWeaponBottomL, posWeaponBottomR, posWeaponTopL, posWeaponTopR;
    if (!FrameGetWeaponPos(playerActor, posWeaponBottomL, posWeaponBottomR, posWeaponTopL, posWeaponTopR, true)) {
        log::warn("Fail to get player's weapon pos");
        return;
    }
    weapPosBuf.Push(WeaponPos(posWeaponBottomL, posWeaponTopL), true);
    weapPosBuf.Push(WeaponPos(posWeaponBottomR, posWeaponTopR), false);

    RE::NiPoint3 posWeaponFixedMiddleL, posWeaponFixedMiddleR, posPlayerHandL, posPlayerHandR; // Fixed point of player's weapon, so velocity calculation is the same for all weapons
    if (!FrameGetWeaponFixedPos(playerActor, posWeaponFixedMiddleL, posWeaponFixedMiddleR, posPlayerHandL, posPlayerHandR)) {
        log::warn("Fail to get player's fixed point pos");
        return;
    }
    speedBuf.Push(WeaponPos(posPlayerHandL, posWeaponFixedMiddleL), true);
    speedBuf.Push(WeaponPos(posPlayerHandR, posWeaponFixedMiddleR), false);

    RE::NiPoint3 leftSpeed = speedBuf.GetVelocity(5, true);
    RE::NiPoint3 rightSpeed = speedBuf.GetVelocity(5, false);

    
    if (playerActor->IsBlocking()) {
        log::trace("Player is blocking");
        return;
    }

    // If bPlayerMustBeAttacking is true, which is default true for SE/AE, and default false for VR
    // Then we return here, because player must be attacking to parry
    bool bAbleToParry = true;
    if (bPlayerMustBeAttacking) {
        if (!playerActor->AsActorState()) {
            log::warn("Fail to get ActorState for player");
            return;
        }
        if (!OnMeleeHit::IsAttacking(playerActor->AsActorState()->GetAttackState())) {
            log::trace("Player not attacking");
            bAbleToParry = false;
        }
    }

    // Get nearby projectiles
    // If the position of player's weapon now, or the positions of player's weapon in the past several frames
    //      is close to the projectile (projectile's one end is its position, and another end is ahead of it using velocity:
    //      (1) mark it as parried
    //      (2) TODO: change owner to player
    //      (3) change velocity to player weapon velocity, but slower. 
    //          If velocity direction is basically to the caster, make it fly to caster
    //      (4) start slow time effect
    std::vector<RE::TESObjectREFR*> vNearbyProj;
    if (const auto TES = RE::TES::GetSingleton(); TES && bEnableProjParry) {
        TES->ForEachReferenceInRange(playerRef, fProjDetectRange, [&](RE::TESObjectREFR& b_ref) {
            // TODO:Seems that the code below doesn't fire every frame!
            auto proj = b_ref.AsProjectile();
            if (!proj) return RE::BSContainer::ForEachResult::kContinue;
            int projType = -1;
            // Ice storm, dragon's fire is none of below and can't be detected currently
            if (b_ref.Is(RE::FormType::ProjectileBeam)) {
                // sparks, chain lightning, can't parry
                // They are present only when close to enemy, and velocity is very high
                projType = 0;
            }
            if (b_ref.Is(RE::FormType::ProjectileFlame)) {
                // frostbite, flame, can't parry.
                // They are present only when close to enemy, and their velocity is 0. Also changing velocity's effect is strange
                projType = 1;
            }
            if (b_ref.Is(RE::FormType::ProjectileCone)) {
                // haven't seen
                projType = 2;
            }
            if (b_ref.Is(RE::FormType::Projectile)) {
                // haven't seen
                projType = 3;
            }
            if (b_ref.Is(RE::FormType::ProjectileArrow)) {
                projType = 4;
            }
            if (b_ref.Is(RE::FormType::ProjectileBarrier)) {
                // haven't seen
                projType = 5;
            }
            if (b_ref.Is(RE::FormType::ProjectileGrenade)) {
                // rune, can't parry. Will stay on ground
                projType = 6;
            }
            if (b_ref.Is(RE::FormType::ProjectileMissile)) {
                // firebelt, icespike
                projType = 7;
            }
            if (projType != 4 && projType != 7) {
                log::trace("Projectile type can't be parried. Name:{}", b_ref.GetName());
                return RE::BSContainer::ForEachResult::kContinue;
            }
            if (!b_ref.Is3DLoaded()) {
                log::trace("Proj 3D not loaded. Name:{}", b_ref.GetName());
                return RE::BSContainer::ForEachResult::kContinue;
                
            }

            vNearbyProj.push_back(&b_ref);
            return RE::BSContainer::ForEachResult::kContinue;
        });
    } else {
        log::warn("Fail to get TES singleton");
    }

    for (auto* b_ref : vNearbyProj) {
        auto proj = b_ref->AsProjectile();
        bool isMissile = b_ref->Is(RE::FormType::ProjectileMissile);
        RE::Projectile::PROJECTILE_RUNTIME_DATA& projRuntime = proj->GetProjectileRuntimeData();
        RE::ObjectRefHandle casterHandle = projRuntime.shooter;
        RE::NiPoint3 velocity = projRuntime.linearVelocity;

        bool isSlowed = parriedProj.IsSlowed(proj) != 99999;  // 99999 means not slowed

        // Check if the velocity is too low
        if (velocity.Length() < 100.0f && !isSlowed) {
            log::trace("Too slow Asprojectile. Name:{}. Speed:{}. Pos:{},{},{}", proj->GetName(), velocity.Length(),
                       proj->GetPositionX(), proj->GetPositionY(), proj->GetPositionZ());
            continue;
        }

        // Check if the projectile already triggered impact
        int countImpact = 0;
        for (auto impactIter = projRuntime.impacts.begin(); impactIter != projRuntime.impacts.end(); ++impactIter) {
            countImpact++;
            auto impact = *impactIter;
            if (impact) {
                log::trace("Impact result:{}", static_cast<int>(impact->impactResult));
            } else {
                log::trace("Impact is nullptr");
            }
        }
        if (countImpact > 0) {
            log::trace("Impact count:{}. Return", countImpact);
            continue;
        }

        // Check if it has caster and caster is not player
        auto caster = casterHandle.get().get();
        if (!caster || caster == playerActor) {
            log::trace("Asprojectile without caster or caster is player. Name:{}. Pos:{},{},{}", proj->GetName(),
                       proj->GetPositionX(), proj->GetPositionY(), proj->GetPositionZ());
            continue;
        }

        // Check if it has already been parried
        if (parriedProj.IsParried(proj)) {
            log::trace("Found parried Asprojectile. Shooter:{}. Name:{}. Speed:{}. Pos:{},{},{}",
                       caster->GetDisplayFullName(), proj->GetName(), velocity.Length(), proj->GetPositionX(),
                       proj->GetPositionY(), proj->GetPositionZ());
            continue;
        }

        log::trace("Found Asprojectile. Name:{}. Speed:{}. Pos:{},{},{}", proj->GetName(), velocity.Length(),
                   proj->GetPositionX(), proj->GetPositionY(), proj->GetPositionZ());

        // Option 1:
        //// Slow the projectile down, just for once, if player's weapon speed is high enough
        // auto distProjPlayer = proj->GetPosition().GetDistance(playerActor->GetPosition());
        // if (parriedProj.IsSlowed(proj) == false && distProjPlayer < fProjSlowRadius &&
        //     (leftSpeed.SqrLength() > fPlayerWeaponSpeedRewardThres2 ||
        //      rightSpeed.SqrLength() > fPlayerWeaponSpeedRewardThres2)) {
        //     // TODO: delete unnecessary angle, velocity,
        //     log::debug("About to slow a projectile. Dist: {}", distProjPlayer);
        //     parriedProj.PushSlowed(proj, projRuntime.linearVelocity, proj->GetAngle());
        //     projRuntime.linearVelocity *= fProjSlowRatio;
        // }

        // Option 2:
        // When the player is pressing the trigger, enters slow motion if the projectile is close enough once
        // However, this alone is not good, because slow motion takes effect a few frames later, not immediately

        // Option 3:
        // When the player is pressing the trigger, slow the projectile down
        auto inputManager = RE::BSInputDeviceManager::GetSingleton();
        auto distProjPlayer = proj->GetPosition().GetDistance(playerActor->GetPosition());
        log::trace("isSlowed:{}, distProjPlayer:{}, inputManager valid:{}", isSlowed, distProjPlayer,
                   inputManager != nullptr);
        if (!isSlowed && distProjPlayer < fProjSlowRadius &&
            inputManager) {  // 99999 means not slowed
            // auto vrControllerL = inputManager->GetVRControllerLeft();
            // auto vrControllerR = inputManager->GetVRControllerRight();
            // auto keyboard = inputManager->GetKeyboard();
            // auto mouse = inputManager->GetMouse();
            // auto virtualKeyboard = inputManager->GetVirtualKeyboard();
            // auto Gamepad = inputManager->GetGamepad();
            // auto GamepadHandler = inputManager->GetGamepadHandler();
            //// All of them below are not null. However, the map size of controllers and virtualKeyboard are 0
            //// Also, even if Gamepad is not null, Gamepad->IsEnabled() can crash the program
            //// The map size of mouse and keboard is very large, but accessing them crashes the program
            // if (vrControllerL)
            //     log::info("vrControllerL not null. Enabled:{}. butNameMapSize:{}. deviceButMapSize:{}",
            //     vrControllerL->IsEnabled(),
            //               vrControllerL->buttonNameIDMap.size(), vrControllerL->deviceButtons.size());
            // if (vrControllerR)
            //     log::info("vrControllerR not null. Enabled:{}. butNameMapSize:{}. deviceButMapSize:{}",
            //     vrControllerR->IsEnabled(),
            //               vrControllerR->buttonNameIDMap.size(), vrControllerR->deviceButtons.size());
            // if (keyboard)
            //     log::info("keyboard not null. Enabled:{}. butNameMapSize:{}. deviceButMapSize:{}",
            //     keyboard->IsEnabled(),
            //               keyboard->buttonNameIDMap.size(), keyboard->deviceButtons.size());
            // if (mouse)
            //     log::info("mouse not null. Enabled:{}. butNameMapSize:{}. deviceButMapSize:{}", mouse->IsEnabled(),
            //               mouse->buttonNameIDMap.size(), mouse->deviceButtons.size());
            // if (virtualKeyboard)
            //     log::info("virtualKeyboard not null. Enabled:{}. butNameMapSize:{}. deviceButMapSize:{}",
            //     virtualKeyboard->IsEnabled(),
            //               virtualKeyboard->buttonNameIDMap.size(), virtualKeyboard->deviceButtons.size());
            log::trace("iFrameCount:{}, iFrameTriggerPress:{}", iFrameCount, iFrameTriggerPress);
            if (iFrameCount - iFrameTriggerPress < 3 && iFrameCount - iFrameTriggerPress >= 0) {
                bool byPassMagCheck = false || (fProjSlowCost == 0);
                if (iFrameCount - iFrameSlowCost < 90 && iFrameCount - iFrameSlowCost >= 0) {
                    byPassMagCheck = true;
                }
                // Check player magicka
                auto curMag = playerActor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka);
                if (curMag >= fProjSlowCost || byPassMagCheck) {
                    if (!byPassMagCheck) {
                        playerActor->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage,
                                                                            RE::ActorValue::kMagicka, -fProjSlowCost);
                        iFrameSlowCost = iFrameCount;
                    }
                    float oriZ = projRuntime.linearVelocity.z;
                    projRuntime.linearVelocity *= fProjSlowRatio;
                    /*projRuntime.linearVelocity.y *= fProjSlowRatio;
                    projRuntime.linearVelocity.z *= fProjSlowRatio;*/
                    log::trace("Trigger pressed. Magicka before cost:{}. Speed after slow down:{}", curMag,
                               formatNiPoint3(projRuntime.linearVelocity));
                    parriedProj.PushSlowed(proj, oriZ);

                } else {
                    log::trace("Trigger pressed. Magicka not enough:{}", curMag);
                }
            }
        }

        if (auto indexSlowed = parriedProj.IsSlowed(proj); indexSlowed != 99999) {
            // If the slow is still effective, compensate the gravity
            auto frameDiff = iFrameCount - parriedProj.bufferFrameSlow[indexSlowed];
            log::trace("Frame diff:{}. iProjSlowFrame:{}", frameDiff, iProjSlowFrame);
            if (frameDiff < iProjSlowFrame && frameDiff > 0) {
                log::trace("Compensate gravity. Current z:{}", projRuntime.linearVelocity.z);
                projRuntime.linearVelocity.z = parriedProj.bufSlowVelZ[indexSlowed] * fProjSlowRatio;
            }

            // If the slow should expire, restore the velocity
            if (frameDiff >= iProjSlowFrame && parriedProj.bufSlowRestored[indexSlowed] == false) {
                projRuntime.linearVelocity /= fProjSlowRatio;
                // projRuntime.linearVelocity.y /= fProjSlowRatio;
                parriedProj.bufSlowRestored[indexSlowed] = true;
                // projRuntime.linearVelocity.z = parriedProj.bufSlowVelZ[indexSlowed];
                log::trace("Restore speed. Final speed:{}", formatNiPoint3(projRuntime.linearVelocity));
            }
        }

        // See if player successfully parries it
        // Missiles or not slowed can have easier parry, since they may explode on player's weapon is Higgs is installed
        if (!bAbleToParry) continue;
        DistResult shortestDist =
            weapPosBuf.ShortestDisRecently(iProjCollisionFrame, proj->GetPosition(), velocity, isSlowed);
        if (shortestDist.dist > fProjCollisionDistThres || (isMissile && shortestDist.dist > fProjCollisionDistThres + 15.0f)) {
            log::trace("Parry projectile no success. Dist:{}", shortestDist.dist);
            log::trace("Left weapon bot:{}. top:{}",
                       formatNiPoint3(weapPosBuf.bufferL[weapPosBuf.indexCurrentL].bottom),
                       formatNiPoint3(weapPosBuf.bufferL[weapPosBuf.indexCurrentL].top));
            log::trace("Right weapon bot:{}. top:{}",
                       formatNiPoint3(weapPosBuf.bufferR[weapPosBuf.indexCurrentR].bottom),
                       formatNiPoint3(weapPosBuf.bufferR[weapPosBuf.indexCurrentR].top));
            continue;
        }

        // Mark as parried, to avoid being parried multiple times
        parriedProj.PushParried(proj);

        auto oriVel = projRuntime.linearVelocity;
        log::trace("Parried projectile! Ori Velocity:{},{},{}", oriVel.x, oriVel.y, oriVel.z);

        //// Option 1: Set the projectile to fly to its caster
        // RE::NiPoint3 vecToCaster = caster->GetPosition() + RE::NiPoint3(0, 0, 50.0f) - proj->GetPosition();
        // if (vecToCaster.Length() > 0.0f) vecToCaster /= vecToCaster.Length();
        // vecToCaster *= oriVel.Length();
        // projRuntime.linearVelocity = vecToCaster;

        // Option 2: Set the projectile to fly to player's weapon direction
        RE::NiPoint3 vecWeapon = shortestDist.proj_isLeft ? leftSpeed : rightSpeed;
        if (vecWeapon.Length() > 0.0f) {
            vecWeapon /= vecWeapon.Length();
            if (oriVel.Length() > 0.0f) {
                vecWeapon += oriVel / oriVel.Length() / 1.5;
            }
        } else {
            vecWeapon = RE::NiPoint3(0, 0, 1.0);
        }
        vecWeapon /= vecWeapon.Length();
        vecWeapon *= oriVel.Length();
        projRuntime.linearVelocity = vecWeapon;
        /*if (projType == 4) {
            projRuntime.linearVelocity *= 0.5f;
        } else if (projType == 7) {
            projRuntime.linearVelocity *= 0.8f;
        } */

        // Deprecated: Trying to change owner to player, but not useful
        // projRuntime.shooter = projRuntime.desiredTarget;
        // projRuntime.desiredTarget = casterHandle;
        // auto oriCause = projRuntime.actorCause.get();
        // if (oriCause) {
        //    oriCause->actor = RE::BSPointerHandle<RE::Actor>(playerActor);
        //} else {
        //    log::warn("Fail to get projectile actorCause");
        //}

        // Deprecated: Trying to shoot a new arrow at enemy, but failed
        /*if (projType == 4) {
            projRuntime.linearVelocity *= 0.05f;
            LaunchArrow(proj, playerActor, caster);
        }*/

        // Trying to disable its spell. Disabling weaponSource and ammoSource seems not very useful
        projRuntime.spell = nullptr;
        projRuntime.explosion = nullptr;
        projRuntime.weaponSource = nullptr;
        projRuntime.ammoSource = nullptr;
        projRuntime.power = 0.0f;
        projRuntime.weaponDamage = 0.0f;

        // Slow time
        TimeSlowEffect(playerActor, iTimeSlowFrameProj);

        // Play sound and spark
        auto nodeName = shortestDist.proj_isLeft ? "SHIELD"sv : "WEAPON"sv;
        if (isPlayerUsingClaw) {
            nodeName = shortestDist.proj_isLeft ? "NPC L Hand [RHnd]"sv : "WEAPON"sv;
        }
        RE::NiPoint3 contactPos = shortestDist.contactPoint;
        auto root = netimmerse_cast<RE::BSFadeNode*>(playerActor->Get3D());
        if (root) {
            auto bone = netimmerse_cast<RE::NiNode*>(root->GetObjectByName(nodeName));
            if (bone) {
                SKSE::GetTaskInterface()->AddTask([playerActor, contactPos, bone]() {
                    RE::NiPoint3 P_V = {0.0f, 0.0f, 0.0f};
                    RE::NiPoint3 contactPosition_tmp = contactPos;
                    OnMeleeHit::play_sound(playerActor, 0x0003C73C);
                    OnMeleeHit::play_impact_2(playerActor, RE::TESForm::LookupByID<RE::BGSImpactData>(0x0004BB54), &P_V,
                                              &contactPosition_tmp, bone);
                });
            }
        }
    }

    if (!bAbleToParry) return;

    // Get nearby enemies and flor/tree
    std::vector<RE::TESObjectREFR*> vNearbyObj;
    std::vector<RE::TESObjectREFR*> vNearbyTree;
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
            logger::trace("A nearby actor. Name:{}. Baseform:{:x}. Race:{:x}", actorNPC->GetBaseObject()->GetName(),
                          actorNPC->GetBaseObject()->formID, actorNPC->GetRace()->formID);

            if (bShowEnemyWeaponSegment) {
                // for debug, we call the function below to get the effect spawning on enemy's weapons
                RE::NiPoint3 tmp1, tmp2, tmp3, tmp4;
                tmp1.x = 1234.0f;
                FrameGetWeaponPos(actorNPC, tmp1, tmp2, tmp3, tmp4, false);
            }
            // See if a recent collision happens to this actor
            Collision* recentCol = colBuffer.GetThisEnemyLatestCollision(actorNPC);
            if (recentCol) {
                if ((*recentCol).shouldIgnoreCollision()) {
                    logger::trace("Ignore collision and push enemy. Enemy:{}", actorNPC->GetBaseObject()->GetName());
                    recentCol->ChangeVelocity();
                    recentCol->ChangeAngle();
                    recentCol->SpawnSpark();
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

            RE::NiPoint3 tmp1, tmp2, posEnemyHandL, posEnemyHandR; 
            if (!FrameGetWeaponFixedPos(actorNPC, tmp1, tmp2, posEnemyHandL, posEnemyHandR)) {
                log::warn("Fail to get enemy's fixed point pos");
                continue;
            }

            // See if player's weapon collides with enemies' weapons
            // For now, we check both the collision between any weapon regardlessly 
            //      (enemy's L and player's L and R, and enemy's R and player's L and R)

            RE::NiPoint3 contactPos; // approximately where collision happens
            float contactToPlayerHand, contactToEnemyHand; // these are used to spawn sparks after collision
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
            
            // If the enemy is special race, change the distThres
            auto distThres = fCollisionDistThres;
            int specialRace = GetSpecialRace(actorNPC);
            switch (specialRace) {
                case 1: // dwenmer sphere
                    if (distThres < 25.0f) distThres = 25.0f;
                    break;
                case 2: // races that only use mouth as weapon: wolf, skeevers
                    if (distThres < 15.0f) distThres = 15.0f;
                    break;
                case 3:  // races that use claws: werewolf or werebear or trolls or bears or hagraven
                    if (distThres < 20.0f) distThres = 20.0f;
                    break;
                case 5: // cats
                    if (distThres < 18.0f) distThres = 18.0f;
                    break;
                case 6: // dwenmer spider
                    if (distThres < 15.0f) distThres = 15.0f;
                    break;
                //case 7:  // frostbite spider
                //    if (distThres < 20.0f) distThres = 20.0f;
                //    break;
                case 0:
                    break;
                case -1:
                    log::error("Fail to get special race because actor or race is null");
                    return;
                default:
                    break;
            }

            // If player is special race, change the distThres
            if (playerActor->GetRace()->formID == werewolfRace ||
                GetBaseFormID(playerActor->GetRace()->formID) == vampLord) {
                if (distThres < 20.0f) distThres = 20.0f;
            }


            // Now detect collision
            if (dis_playerL_enemyL.dist < distThres) {
                isCollision = true;
                isEnemyLeft = true;
                isPlayerLeft = true;
                contactPos = dis_playerL_enemyL.contactPoint;
                contactToPlayerHand = contactPos.GetDistance(posPlayerHandL);
                contactToEnemyHand = contactPos.GetDistance(posEnemyHandL);
            } else if (dis_playerL_enemyR.dist < distThres) {
                isCollision = true;
                isPlayerLeft = true;
                contactPos = dis_playerL_enemyR.contactPoint;
                contactToPlayerHand = contactPos.GetDistance(posPlayerHandL);
                contactToEnemyHand = contactPos.GetDistance(posEnemyHandR);
            } else if (dis_playerR_enemyL.dist < distThres) {
                isCollision = true;
                isEnemyLeft = true;
                contactPos = dis_playerR_enemyL.contactPoint;
                contactToPlayerHand = contactPos.GetDistance(posPlayerHandR);
                contactToEnemyHand = contactPos.GetDistance(posEnemyHandL);
            } else if (dis_playerR_enemyR.dist < distThres) {
                isCollision = true;
                contactPos = dis_playerR_enemyR.contactPoint;
                contactToPlayerHand = contactPos.GetDistance(posPlayerHandR);
                contactToEnemyHand = contactPos.GetDistance(posEnemyHandR);
            } 
            if (isCollision) {

                if (fPlayerStaUnableParryThresPer > 0.0f && playerActor->AsActorValueOwner()) {
                    auto playerCurSta = playerActor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
                    if (playerCurSta <
                        fPlayerStaUnableParryThresPer *
                            playerActor->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kStamina)) {
                        log::trace("Player current stamina is not enough for parry. Stamina:{}", playerCurSta);
                        RE::DebugNotification("Stamina not enough for parry");
                        continue;
                    }
                }

                // create a new collision
                auto playerWeapSpeed = isPlayerLeft ? leftSpeed : rightSpeed;
                auto speed = playerWeapSpeed.SqrLength();

                // Check if collision with shield
                bool isBlocking = false;
                if (bEnableShieldCollision && bTreatShieldAsParry == false) {
                    if (auto equipL = playerActor->GetEquippedObject(true); equipL && equipL->IsArmor()) {
                        isBlocking = true;
                    }
                }

                // push the enemy away by this velocity. Will be used for the next few frames
                RE::hkVector4 pushEnemyVelocity =
                    CalculatePushVector(playerActor->GetPosition(), actorNPC->GetPosition(), true, speed);

                // get the angle of enemy. Will be used for the next few frames
                float angleZ = 0.0f;
                if (auto enemyRef = static_cast<RE::TESObjectREFR*>(actorNPC); enemyRef) {
                    angleZ = enemyRef->GetAngleZ();
                }
                bool isRotClockwise = ShouldRotateClockwise(playerActor->GetPosition(), actorNPC->GetPosition(), playerWeapSpeed);
                int64_t rotDurationFrame = RotateFrame(playerWeapSpeed.SqrLength());

                // If blocking, there should be no pushback, rotation
                if (isBlocking) {
                    rotDurationFrame = 0;
                    pushEnemyVelocity = pushEnemyVelocity * 0;
                }

                auto col = Collision(actorNPC, iFrameCount, actorNPC->GetPosition(),
                                     CalculatePushDist(true, speed), contactToEnemyHand, isEnemyLeft,
                                     pushEnemyVelocity, angleZ,
                                     isRotClockwise, rotDurationFrame);
                log::debug("Successfully parried {} on frame {}", col.getEnemy()->GetBaseObject()->GetName(),
                           col.getFrame());
                log::debug("Player weapon isLeft:{}, lBot:{}, lTop:{}", isPlayerLeft,
                           formatNiPoint3(posWeaponBottomL), formatNiPoint3(posWeaponTopL));
                log::debug("Player weapon rBot:{}, rTop:{}", formatNiPoint3(posWeaponBottomR),
                           formatNiPoint3(posWeaponTopR));
                log::debug("Enemy weapon isLeft:{}, lBot:{}, lTop:{}", isEnemyLeft,
                           formatNiPoint3(posNPCWeaponBottomL), formatNiPoint3(posNPCWeaponTopL));
                log::debug("Enemy weapon rBot:{}, rTop:{}", formatNiPoint3(posNPCWeaponBottomR),
                           formatNiPoint3(posNPCWeaponTopR));
                colBuffer.PushCopy(col);

                // update PlayerCollision to record this as the latest collision
                static PlayerCollision* latestCol = PlayerCollision::GetSingleton();
                RE::hkVector4 pushPlayerVelocity =
                    CalculatePushVector(actorNPC->GetPosition(), playerActor->GetPosition(), false, speed);

                if (isBlocking) pushPlayerVelocity = pushPlayerVelocity * 0;

                latestCol->SetValue(iFrameCount, contactToPlayerHand, isPlayerLeft, pushPlayerVelocity, 10,
                                    CalculatePushDist(false, speed), playerActor->GetPosition(), playerActor);

                // Collision effects
                CollisionEffect(playerActor, actorNPC, contactPos, isEnemyLeft, isPlayerLeft);
            }
        }
    }
}

void ZacOnFrame::TimeSlowEffect(RE::Actor* playerActor, int64_t slowFrame) { 
    if (slowFrame <= 0 || fTimeSlowRatio >= 1.0f || fTimeSlowRatio <= 0.0f) {
        log::trace("Due to settings, no time slow effect");
        return;
    }

    // TODO: have a different frame counter for projectile
    if (iFrameCount - slowTimeData.frameLastSlowTime < 50 && iFrameCount - slowTimeData.frameLastSlowTime > 0) {
        log::trace("The last slow time effect is within 50 frames. Don't slow time now");
        return;
    }

    if (slowTimeData.timeSlowSpell == nullptr) {
        RE::SpellItem* timeSlowSpell = GetTimeSlowSpell_SpeelWheel();
        if (!timeSlowSpell) {
            timeSlowSpell = GetTimeSlowSpell_Mine();
            if (!timeSlowSpell) {
                log::trace("TimeSlowEffect: failed to get timeslow spell");
                return;
            }
        }
        slowTimeData.timeSlowSpell = timeSlowSpell;
    }

    if (playerActor->HasSpell(slowTimeData.timeSlowSpell)) {
        log::trace("TimeSlowEffect: Spell wheel or us are already slowing time, don't do anything");
        return;
    }
    
    if (slowTimeData.oldMagnitude.size() > 0) {
        log::error("TimeSlowEffect: slowTimeData.oldMagnitude.size() is not zero:{}", slowTimeData.oldMagnitude.size());
        return;
    }

    // Record spell magnitude
    if (slowTimeData.timeSlowSpell->effects.size() == 0) {
        log::error("TimeSlowEffect: timeslow spell has 0 effect");
        return;
    }

    slowTimeData.frameShouldRemove = iFrameCount + slowFrame;
    slowTimeData.frameLastSlowTime = iFrameCount;

    for (RE::BSTArrayBase::size_type i = 0; i < slowTimeData.timeSlowSpell->effects.size(); i++) {
        auto effect = slowTimeData.timeSlowSpell->effects.operator[](i);
        if (!effect) {
            log::trace("TimeSlowEffect: effect[{}] is null", i);
            continue;
        }
        float oldmag = effect->effectItem.magnitude;
        slowTimeData.oldMagnitude.push_back(oldmag); // store the existing magnitude, which are set by spell wheel
        effect->effectItem.magnitude = fTimeSlowRatio; // set our magnitude
    }

    // Apply spell to player. Now time slow will take effect
    playerActor->AddSpell(slowTimeData.timeSlowSpell);
    log::trace("time slow takes effect", fTimeSlowRatio);
}

void ZacOnFrame::StopTimeSlowEffect(RE::Actor* playerActor) {
    if (slowTimeData.timeSlowSpell == nullptr) {
        log::error("StopTimeSlowEffect: timeslow spell is null. Doesn't make sense");
        return;
    }

    if (!playerActor->HasSpell(slowTimeData.timeSlowSpell)) {
        log::warn("StopTimeSlowEffect: player already doesn't have timeslow spell. Probably removed by spellwheel");
        slowTimeData.clear();
        return;
    }

    // Remove spell from player. Now time slow should stop
    playerActor->RemoveSpell(slowTimeData.timeSlowSpell);
    log::trace("time slow stops", fTimeSlowRatio);

    // Restore the effects to the magnitude that spellwheel set
    if (slowTimeData.timeSlowSpell->effects.size() == 0) {
        log::error("StopTimeSlowEffect: timeslow spell has 0 effect");
        return;
    }
    for (RE::BSTArrayBase::size_type i = 0; i < slowTimeData.timeSlowSpell->effects.size(); i++) {
        auto effect = slowTimeData.timeSlowSpell->effects.operator[](i);
        if (!effect) {
            log::trace("StopTimeSlowEffect: effect[{}] is null", i);
            continue;
        }
        if (i < slowTimeData.oldMagnitude.size() && i >= 0) {
            effect->effectItem.magnitude = slowTimeData.oldMagnitude[i];
        } else {
            log::error("StopTimeSlowEffect: out of boundary. Index: {}. size:{}", i, slowTimeData.oldMagnitude.size());
        }
    }

    slowTimeData.clear();
}

void ZacOnFrame::CollisionEffect(RE::Actor* playerActor, RE::Actor* enemyActor, RE::NiPoint3 contactPos, bool isEnemyLeft, bool isPlayerLeft) {
   
    auto nodeName = isPlayerLeft?
        "SHIELD"sv: "WEAPON"sv;
    bool isPlayerUsingClaw = false;
    if (GetSpecialRace(playerActor) == 3) {
        nodeName = isPlayerLeft ? "NPC L Hand [RHnd]"sv : "WEAPON"sv;
        isPlayerUsingClaw = true;
    }
    auto rootEnemy = netimmerse_cast<RE::BSFadeNode*>(enemyActor->Get3D());
    if (!rootEnemy) return;
    auto rootPlayer = netimmerse_cast<RE::BSFadeNode*>(playerActor->Get3D());
    if (!rootPlayer) return;
    auto bonePlayer = netimmerse_cast<RE::NiNode*>(rootPlayer->GetObjectByName(nodeName));
    if (!bonePlayer) return;
    const auto nodeNameFoot = "NPC L Toe0 [LToe]"sv;
    auto boneFoot = netimmerse_cast<RE::NiNode*>(rootEnemy->GetObjectByName(nodeNameFoot));

    bDisableSpark = false;
    // Display spark and sound
    if (!bSparkForBeast) {
        if (isPlayerUsingClaw) bDisableSpark = true;
        switch (GetSpecialRace(enemyActor)) { 
            case 2:
            case 3:
            case 5:
                bDisableSpark = true;
                break;
            default:
                break;
        }
    }

    SKSE::GetTaskInterface()->AddTask([playerActor, enemyActor, contactPos, bonePlayer, boneFoot]() {
        RE::NiPoint3 contact = contactPos;
        RE::NiPoint3 P_V = {0.0f, 0.0f, 0.0f};
        OnMeleeHit::play_sound(enemyActor, 0x0003C73C);
        // Display spark on enemy's weapon, at the collision point
        if (!playerActor->Is3DLoaded()) return;

        if (!bDisableSpark) {
            OnMeleeHit::play_impact_2(playerActor, RE::TESForm::LookupByID<RE::BGSImpactData>(0x0004BB54), &P_V,
                                      &contact, bonePlayer);
        }

        if (!boneFoot) return;
        // Display dust under enemy's foot
        RE::NiPoint3 foot = boneFoot->world.translate;
        // 13CBA, WPNBlade1HandVsDirtImpact; 
        if (!enemyActor->Is3DLoaded()) return;
        OnMeleeHit::play_impact_2(enemyActor, RE::TESForm::LookupByID<RE::BGSImpactData>(0x00013CBA), &P_V, &foot,
                                  boneFoot);
    });

    // Check if is Shield
    // Deprecated: playerActor->NotifyAnimationGraph("blockStart"sv) can make player move slower and kinda blocks
    // However, it doesn't make shield shake or enemy recoil as blocking should do. Also, damage taken is between hit and correct blocking
    if (bEnableShieldCollision && bTreatShieldAsParry == false) {
        if (auto equipL = playerActor->GetEquippedObject(true); equipL && equipL->IsArmor()) {

            
            bool bNotifyResult = playerActor->NotifyAnimationGraph("blockStart"sv);
            log::debug("Processing shield collision as block. Animation notify result:{}", bNotifyResult);
            iFrameStopBlock = iFrameCount + iFrameBlockDur;

            //// Change block config
            // Deprecated: "blockEnter null:true, blockExit null:true"
            //auto iniHandler = RE::INISettingCollection::GetSingleton();
            //if (iniHandler) {

            //    auto blockEnter = iniHandler->GetSetting("fVRShieldBlockEnterAngle");
            //    auto blockExit = iniHandler->GetSetting("fVRShieldBlockExitAngle");
            //    if (blockEnter && blockExit) {
            //        log::debug("Prev Shield enter angle:{}", blockEnter->data.f);
            //        log::debug("Prev Shield exit angle:{}", blockExit->data.f);
            //        fOriginEnterAngle = blockEnter->data.f;
            //        fOriginExitAngle = blockExit->data.f;

            //        blockExit->data.f = 180.0f;
            //        iniHandler->WriteSetting(blockExit);

            //    } else {
            //        log::warn("blockEnter null:{}, blockExit null:{}", blockEnter == nullptr, blockExit == nullptr);
            //    }


            //} else {
            //    log::warn("iniHandler is null!");
            //}


            return;
        }
    }
    

    // if enemy is power attacking, double sta cost to player
    bool isEnemyPower = false;
    if (auto enemyAI = enemyActor->GetActorRuntimeData().currentProcess; enemyAI) {
        if (enemyAI->high && enemyAI->high->attackData) {
            if (enemyAI->high->attackData->data.flags.any(RE::AttackData::AttackFlag::kPowerAttack)) {
                isEnemyPower = true;
            }
            log::trace("Enemy attack damageMult:{}. Is power:{}", enemyAI->high->attackData->data.damageMult, isEnemyPower);
            
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
    uint16_t playerDamage(5), enemyDamage(5), oriDmg(0);  // 5 for unhanded
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
            oriDmg = enemyDamage;
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
        }
    }
    auto enemyActorMeleeDamage =
        enemyActor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMeleeDamage) *
        enemyActor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kAttackDamageMult) * 0.7;  // for special races, but seems 0
    enemyDamage = enemyDamage > enemyActorMeleeDamage ? enemyDamage : enemyActorMeleeDamage;
    auto enemyActorUnArmedDamage =
        enemyActor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kUnarmedDamage) *
                                   enemyActor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kAttackDamageMult) *
                                   0.7;  // for special races. 
    enemyDamage = enemyDamage > enemyActorUnArmedDamage ? enemyDamage : enemyActorUnArmedDamage;
    log::trace("Damage of enemy. Final: {}. Ori: {}. Melee:{}. Unarmed:{}", enemyDamage, oriDmg, enemyActorMeleeDamage,
               enemyActorUnArmedDamage);

    float enemyStaCost = fEnemyStaCostWeapMulti * (float) ( 2.0 * playerDamage - enemyDamage);
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


    // Enemy recoil based on player's weapon speed and their remaining stamina
    auto speed = speedBuf.GetVelocity(5, isPlayerLeft).SqrLength();
    log::trace("Player moving their weapon at speed:{}", speed);
    if (isPlayerPower || speed > fEnemyLargeRecoilVelocityThres) {
        RecoilEffect(enemyActor, 2);
        TimeSlowEffect(playerActor, iTimeSlowFrameLargeRecoil);
    } else if (speed > fEnemyStopVelocityThres) {
        RecoilEffect(enemyActor, 1);
        TimeSlowEffect(playerActor, iTimeSlowFrameStop);
    } else if (enemyFinalSta < // Enemy recoil based on stamina
        fEnemyStaLargeRecoilThresPer *
                            enemyActor->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kStamina)) {
        RecoilEffect(enemyActor, 2);
        TimeSlowEffect(playerActor, iTimeSlowFrameLargeRecoil);
    } else if (enemyFinalSta < fEnemyStaStopThresPer *
                                   enemyActor->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kStamina)) {
        RecoilEffect(enemyActor, 1);
        TimeSlowEffect(playerActor, iTimeSlowFrameStop);
    } else {
        TimeSlowEffect(playerActor, iTimeSlowFrameNormal);
    }

    // Player stamina cost
    auto playerCurSta = playerActor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
    float playerStaCost = fPlayerStaCostWeapMulti * (float)(2.0 * enemyDamage - playerDamage);
    if (isEnemyPower && !isPlayerPower) playerStaCost *= 2;
    playerStaCost = playerStaCost < fPlayerStaCostMin ? fPlayerStaCostMin : playerStaCost;
    playerStaCost = playerStaCost > fPlayerStaCostMax ? fPlayerStaCostMax : playerStaCost;
    float playerFinalSta = playerCurSta - playerStaCost;
    if (playerFinalSta < 0.0f) {
            playerFinalSta = 0.0f;
    }
    playerStaCost = playerCurSta - playerFinalSta;
    // if player is moving their weapon, reduce stamina cost

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

        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

        log::trace("Calling papyrus");
        if (papyrusVM->TypeIsValid("VRIK"sv)) {
            log::trace("VRIK is installed");
            RE::BSScript::IFunctionArguments* hapticArgs;
            if (isPlayerLeft) {
                hapticArgs = RE::MakeFunctionArguments(true, (int)hapticFrame, (int)iHapticLengthMicroSec);
            } else {
                hapticArgs = RE::MakeFunctionArguments(false, (int)hapticFrame, (int)iHapticLengthMicroSec);
            }
            // Function VrikHapticPulse(Bool onLeftHand, Int frames, Int microsec) native global
            papyrusVM->DispatchStaticCall("VRIK"sv, "VrikHapticPulse"sv, hapticArgs, callback);
        } else {
            log::trace("VRIK not installed");
            // Now we call vanilla's script Game.ShakeController(float afLeftStrength, float afRightStrength, float afDuration)
            // afLeftStrength: The strength of the left motor. Clamped from 0 to 1.
            // afRightStrength: The strength of the right motor.Clamped from 0 to 1. 
            // afDuration : How long to shake the controller - in seconds.
            if (papyrusVM->TypeIsValid("Game"sv)) {
                RE::BSScript::IFunctionArguments* hapticArgs;
                if (isPlayerLeft) {
                    hapticArgs = RE::MakeFunctionArguments(((float)hapticFrame) / ((float)iHapticStrMax), 0.0f,
                                                           (float)iHapticLengthMicroSec / 1000000);
                } else {
                    hapticArgs = RE::MakeFunctionArguments(0.0f, ((float)hapticFrame) / ((float)iHapticStrMax), 
                                                           (float)iHapticLengthMicroSec / 1000000);
                }
                papyrusVM->DispatchStaticCall("Game"sv, "ShakeController"sv, hapticArgs, callback);
            } else {
                log::trace("Failed to find vanilla Game script");
            }
        }

        log::trace("Finished calling papyrus");
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
    if (iFrameCount % 3 == 0 && iFrameCount % 6 != 0) {
        OnMeleeHit::play_impact_2(actor, RE::TESForm::LookupByID<RE::BGSImpactData>(0x0004BB52), &P_V, &posWeaponTop,
                                  bone);
    } else if (iFrameCount % 3 == 0 && iFrameCount % 6 == 0) {
        OnMeleeHit::play_impact_2(actor, RE::TESForm::LookupByID<RE::BGSImpactData>(0x0004BB52), &P_V, &posWeaponBottom,
                                  bone);
    }
    
    
}


// This function modifies the 4 NiPoint3 passed in. If failed to get the 3D of actor, it return false.
// Otherwise, it returns true, even if some NiPoint3 are not modified. 
// Caller needs to detect if they are modified, by calling IsNiPointZero()
bool ZacOnFrame::FrameGetWeaponPos(RE::Actor* actor, RE::NiPoint3& posWeaponBottomL, RE::NiPoint3& posWeaponBottomR,
                       RE::NiPoint3& posWeaponTopL,
                       RE::NiPoint3& posWeaponTopR, bool isPlayer) {
    bool bDisplayEnemySegmentCheck = posWeaponBottomL.x == 1234.0f; // a magic number to prevent showing effect twice

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
        isDaggerL(false), isDaggerR(false), hasGauntlet(false);
    if (isPlayer) {
        // if player is wearing any gauntlet
        auto armo = actor->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kHands);
        if (armo) {
            log::trace("Player hands armor:{}", armo->GetFullName());
            if (bPlayerCheckHeavyGauntlet && armo->IsHeavyArmor()) {
                hasGauntlet = true;
            }
        } else {
            armo = actor->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kForearms);
            if (armo) {
                log::trace("Player forearms armor:{}", armo->GetFullName());
                if (bPlayerCheckHeavyGauntlet && armo->IsHeavyArmor()) {
                    hasGauntlet = true;
                }
            }
        }
    }
    if (!bPlayerCheckHeavyGauntlet) {
        hasGauntlet = true;
    }

    if (auto equipR = actor->GetEquippedObject(false); equipR) {
        isEquipR = true;
        if (auto equipWeapR = equipR->As<RE::TESObjectWEAP>(); equipWeapR) {
            log::trace("Player equipWeapR form:{}. Is hand:{}", equipWeapR->formID, equipWeapR->IsHandToHandMelee());
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
            } else if (equipWeapR->IsHandToHandMelee()) {
                if (isPlayer) {
                    // if player is wearing any gauntlet, set fist to true
                    isFistR = hasGauntlet;
                } 
            }
        }
    } else {
        if (isPlayer) {
            // if player is wearing any gauntlet, set fist to true
            isFistR = hasGauntlet;
        } else {
            // We have more restrict ways to check if enemy is using fist
            RE::AIProcess* const attackerAI = actor->GetActorRuntimeData().currentProcess;
            if (attackerAI) {
                const RE::TESForm* equipped = attackerAI->GetEquippedRightHand();
                if (equipped) {
                    auto weapHand = equipped->As<RE::TESObjectWEAP>();
                    if (weapHand && weapHand->IsHandToHandMelee()) {
                        isFistR = true;
                    }
                }
            }
        }
    }
    if (auto equipL = actor->GetEquippedObject(true); equipL) {
        isEquipL = true;
        if (auto equipWeapL = equipL->As<RE::TESObjectWEAP>(); equipWeapL) {
            log::trace("Player equipWeaLR form:{}. Is hand:{}", equipWeapL->formID, equipWeapL->IsHandToHandMelee());
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
            } else if (equipWeapL->IsHandToHandMelee()) {
                if (isPlayer) {
                    // if player is wearing any gauntlet, set fist to true
                    isFistL = hasGauntlet;
                }
            }
        }
        if (equipL->IsArmor()) {
            hasShield = true;
        }
    } else {
        if (isPlayer) {
            // if player is wearing any gauntlet, set fist to true
            isFistL = hasGauntlet;
        } else {
            // We have more restrict ways to check if enemy is using fist
            RE::AIProcess* const attackerAI = actor->GetActorRuntimeData().currentProcess;
            if (attackerAI) {
                const RE::TESForm* equipped = attackerAI->GetEquippedLeftHand();
                if (equipped) {
                    auto weapHand = equipped->As<RE::TESObjectWEAP>();
                    if (weapHand && weapHand->IsHandToHandMelee()) {
                        isFistL = true;
                    }
                }
            }
        }
    }

    log::trace("isFistL:{}, isFistR:{}", isFistL, isFistR);

    // Handling some special actors
    // 1. Dwenmer sphere. Gotta say this is still not good: the adjust matrix works fine when they stand, but when they attack their weapon is quite off
    auto weapDSphere = HandleDwenmerSphere(actor, posWeaponBottomL, posWeaponBottomR, posWeaponTopL, posWeaponTopR);
    if (weapDSphere) { 
        if ((bShowEnemyWeaponSegment && !isPlayer && bDisplayEnemySegmentCheck) || (isPlayer && bShowPlayerWeaponSegment)) {
            debug_show_weapon_range(actor, posWeaponBottomR, posWeaponTopR, weapDSphere);
        }
    }
    // 2. Races that use mouth as their weapon: wolf, skeever
    auto weapWolfHead = HandleMouthRace(actor, posWeaponBottomL, posWeaponBottomR, posWeaponTopL, posWeaponTopR);
    if (weapWolfHead) {
        if ((bShowEnemyWeaponSegment && !isPlayer && bDisplayEnemySegmentCheck) || (isPlayer && bShowPlayerWeaponSegment)) {
            debug_show_weapon_range(actor, posWeaponBottomR, posWeaponTopR, weapWolfHead);
        }
    }
    // 3. Races that use claws as weapon Werewolf or werebear or troll or bear or spriggan or hagraven
    twoNodes claws = HandleClawRaces(actor, posWeaponBottomL, posWeaponBottomR, posWeaponTopL, posWeaponTopR);
    if (!claws.isEmpty()) {
        if ((bShowEnemyWeaponSegment && !isPlayer && bDisplayEnemySegmentCheck) || (isPlayer && bShowPlayerWeaponSegment)) {
            debug_show_weapon_range(actor, posWeaponBottomR, posWeaponTopR, claws.nodeR);
            debug_show_weapon_range(actor, posWeaponBottomL, posWeaponTopL, claws.nodeL);
        }
    }
    // 4. Dwenmer Centurion. Can't parry
    if (GetSpecialRace(actor) == 4) {
        return true;
    }
    // 5. Cats. Use their right claw and head as weapons
    twoNodes clawAndHead =
        HandleClawAndHeadRaces(actor, posWeaponBottomL, posWeaponBottomR, posWeaponTopL, posWeaponTopR);
    if (!clawAndHead.isEmpty()) {
        if ((bShowEnemyWeaponSegment && !isPlayer && bDisplayEnemySegmentCheck) || (isPlayer && bShowPlayerWeaponSegment)) {
            debug_show_weapon_range(actor, posWeaponBottomR, posWeaponTopR, clawAndHead.nodeR);
            debug_show_weapon_range(actor, posWeaponBottomL, posWeaponTopL, clawAndHead.nodeL);
        }
    }
    // 6. Dwenmer spider. Use their right leg and a right pincher as weapons. left let and pincher can't be parried
    twoNodes DWorkerLegs = HandleDwenmerSpider(actor, posWeaponBottomL, posWeaponBottomR, posWeaponTopL, posWeaponTopR);
    if (!DWorkerLegs.isEmpty()) {
        if ((bShowEnemyWeaponSegment && !isPlayer && bDisplayEnemySegmentCheck) || (isPlayer && bShowPlayerWeaponSegment)) {
            debug_show_weapon_range(actor, posWeaponBottomR, posWeaponTopR, DWorkerLegs.nodeR);
            debug_show_weapon_range(actor, posWeaponBottomL, posWeaponTopL, DWorkerLegs.nodeL);
        }
    }
    // 7. Frostbite spider. Use their right claw and mouth as weapons. left claw can't be parried
    // Disabled. Feels strange in game to parry their mouth
    twoNodes FSpiderClaws = twoNodes(nullptr, nullptr);
    //twoNodes FSpiderClaws = HandleFrostSpider(actor, posWeaponBottomL, posWeaponBottomR, posWeaponTopL, posWeaponTopR);
    //if (!FSpiderClaws.isEmpty()) {
    //    if ((bShowEnemyWeaponSegment && !isPlayer && bDisplayEnemySegmentCheck) || (isPlayer && bShowPlayerWeaponSegment)) {
    //        debug_show_weapon_range(actor, posWeaponBottomR, posWeaponTopR, FSpiderClaws.nodeR);
    //        debug_show_weapon_range(actor, posWeaponBottomL, posWeaponTopL, FSpiderClaws.nodeL);
    //    }
    //}


    // Handling normal humanoid actors
    if (!weapDSphere && !weapWolfHead && claws.isEmpty() && clawAndHead.isEmpty() && DWorkerLegs.isEmpty() &&
        FSpiderClaws.isEmpty() && weaponL 
        &&
        (isEquipL || (isFistL && isFistR) || (isPlayer && bEnableShieldCollision && hasShield)) &&
        !(!isPlayer && isBow) && !(!isPlayer && isStaffL)  // ignore enemy's bow and staff
        ) { // only enable fist collision when both hands are fist
        float reachL(0.0f), handleL(0.0f);
        if (isFistL) { 
            reachL = 10.0f;
            handleL = 10.0f;
            log::trace("Left: fist. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isBow) { // OK
            reachL = 70.0f;
            handleL = 70.0f;
            log::trace("Left: bow. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isDaggerL) { // OK
            reachL = 20.0f;
            handleL = 10.0f;
            log::trace("Left: dagger. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isSwordL) { // OK
            reachL = 90.0f;
            handleL = 12.0f;
            log::trace("Left: sword. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isAxeL) { // OK
            reachL = 60.0f;
            handleL = 12.0f;
            log::trace("Left: axe. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isMaceL) { // OK
            reachL = 60.0f;
            handleL = 12.0f;
            log::trace("Left: mace. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isStaffL) { // OK
            reachL = 70.0f;
            handleL = 70.0f;
            log::trace("Left: staff. actor:{}", actor->GetBaseObject()->GetName());
        } else if (hasShield) {
            reachL = fShieldRadius;
            handleL = fShieldRadius;
            log::trace("Left: Shield. actor:{}", actor->GetBaseObject()->GetName());
        } else {
            log::trace("Left: unknown. actor:{}", actor->GetBaseObject()->GetName());
        }
        if (reachL > 0.0f) {
            reachL *= fRangeMulti;
            handleL *= fRangeMulti;
            posWeaponBottomL = weaponL->world.translate;
            const auto weaponDirectionL =
                RE::NiPoint3{weaponL->world.rotate.entry[0][1], weaponL->world.rotate.entry[1][1],
                             weaponL->world.rotate.entry[2][1]};
            posWeaponTopL = posWeaponBottomL + weaponDirectionL * reachL;
            posWeaponBottomL = posWeaponBottomL - weaponDirectionL * handleL;

            if ((bShowPlayerWeaponSegment && isPlayer) ||
                (bShowEnemyWeaponSegment && !isPlayer && bDisplayEnemySegmentCheck))
                // 1234.0f is a magic number to prevent showing effects twice
                debug_show_weapon_range(actor, posWeaponBottomL, posWeaponTopL, weaponL);
        }

    } else {
        // Animals, dragons will reach here
        log::trace("Doesn't get left weapon. actor:{}", actor->GetBaseObject()->GetName());
        
    }
    
    if (!weapDSphere && !weapWolfHead && claws.isEmpty() && clawAndHead.isEmpty() && DWorkerLegs.isEmpty() &&
        FSpiderClaws.isEmpty()
        && weaponR && 
        (isEquipR || (isFistL && isFistR)) && 
        !(!isPlayer && isBow) && !(!isPlayer && isStaffR)
        ) {
        float reachR(0.0f), handleR(0.0f);
        if (isFistR) {
            reachR = 10.0f;
            handleR = 10.0f;
            log::trace("Right: fist. actor:{}", actor->GetBaseObject()->GetName());
        }else if (isTwoHandedAxe || isWarHammer) { // OK
            reachR = 70.0f;
            handleR = 45.0f;
            log::trace("Right: two-handed axe/hammer. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isGreatSword) { // OK
            reachR = 100.0f;
            handleR = 20.0f;
            log::trace("Right: two-handed greatsword. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isDaggerR) { // OK
            reachR = 20.0f;
            handleR = 10.0f;
            log::trace("Right: dagger. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isSwordR) {  // OK
            reachR = 90.0f;
            handleR = 12.0f;
            log::trace("Right: sword. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isAxeR) {  // OK
            reachR = 60.0f;
            handleR = 12.0f;
            log::trace("Right: axe. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isMaceR) {  // OK
            reachR = 60.0f;
            handleR = 12.0f;
            log::trace("Right: mace. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isStaffR) {  // OK
            reachR = 70.0f;
            handleR = 70.0f;
            log::trace("Right: staff. actor:{}", actor->GetBaseObject()->GetName());
        } else {
            log::trace("Right: unknown. actor:{}", actor->GetBaseObject()->GetName());
        }
        if (reachR > 0.0f) {
            reachR *= fRangeMulti;
            handleR *= fRangeMulti;
            posWeaponBottomR = weaponR->world.translate;
            const auto weaponDirectionR =
                RE::NiPoint3{weaponR->world.rotate.entry[0][1], weaponR->world.rotate.entry[1][1],
                             weaponR->world.rotate.entry[2][1]};
            posWeaponTopR = posWeaponBottomR + weaponDirectionR * reachR;
            posWeaponBottomR = posWeaponBottomR - weaponDirectionR * handleR;

            if ((bShowPlayerWeaponSegment && isPlayer) ||
                (bShowEnemyWeaponSegment && !isPlayer && bDisplayEnemySegmentCheck)) {
                debug_show_weapon_range(actor, posWeaponBottomR, posWeaponTopR, weaponR);
            }
        }
    } else {
        log::trace("Doesn't get right weapon. actor:{}", actor->GetBaseObject()->GetName());
    }

    
    return true;
}

bool ZacOnFrame::FrameGetWeaponFixedPos(RE::Actor* actor, RE::NiPoint3& posWeaponTopL, RE::NiPoint3& posWeaponTopR,
                                        RE::NiPoint3& posHandL, RE::NiPoint3& posHandR) {
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
    bool isEquipL(false), isEquipR(false), isFistL(false), isFistR(false), hasShield(false);
    if (auto equipR = actor->GetEquippedObject(false); equipR) {
        isEquipR = true;
    } else {
        // fist
        isFistR = true;
    }
    if (auto equipL = actor->GetEquippedObject(true); equipL) {
        isEquipL = true;
        if (equipL->IsArmor()) {
            hasShield = true;
        }
    } else {
        // fist
        isFistL = true;
    }

    // Handling werewolf and vampire lord for player
    twoNodes claws = HandleClawRaces(actor, posHandL, posHandR, posWeaponTopL, posWeaponTopR);

    if (claws.isEmpty() && weaponL &&
        (isEquipL || (isFistL && isFistR) || (bEnableShieldCollision && hasShield))) {  // only enable fist collision when both hands are fist
        float reachL(70.0f);
        
        posHandL = weaponL->world.translate;
        const auto weaponDirectionL = RE::NiPoint3{weaponL->world.rotate.entry[0][1], weaponL->world.rotate.entry[1][1],
                                                   weaponL->world.rotate.entry[2][1]};
        posWeaponTopL = posHandL + weaponDirectionL * reachL;


    } else {
        // Animals, dragons will reach here
    }

    if (claws.isEmpty() && weaponR && (isEquipR || (isFistL && isFistR))) {
        float reachR(70.0f);
   
        reachR *= fRangeMulti;
        posHandR = weaponR->world.translate;
        const auto weaponDirectionR = RE::NiPoint3{weaponR->world.rotate.entry[0][1], weaponR->world.rotate.entry[1][1],
                                                   weaponR->world.rotate.entry[2][1]};
        posWeaponTopR = posHandR + weaponDirectionR * reachR;

    } else {
        
    }

    return true;
}

RE::hkVector4 ZacOnFrame::CalculatePushVector(RE::NiPoint3 sourcePos, RE::NiPoint3 targetPos, bool isEnemy, float speed) {
    RE::NiPoint3 pushDirection = targetPos - sourcePos;
    // Normalize the direction
    float length =
        sqrt(pushDirection.x * pushDirection.x + pushDirection.y * pushDirection.y);
    if (length > 0.0f) {  // avoid division by zero
        pushDirection.x /= length;
        pushDirection.y /= length; 
    }
    float pushMulti = isEnemy ? fEnemyPushVelocityMulti : fPlayerPushVelocityMulti;
    pushMulti *= 70.0f;

    // modify pushMulti by the speed of player's weapon at collision
    float speedFactor;
    if (speed < 20.0f) {
        speedFactor = 0.1f;
    } else if (speed < 40.0f) {
        speedFactor = 0.3f;
    } else if (speed < 60.0f) {
        speedFactor = 0.5f;
    } else if (speed < 80.0f) {
        speedFactor = 0.7f;
    } else if (speed < 100.0f) {
        speedFactor = 0.9f;
    } else {
        speedFactor = 1.0f;
    }
    if (!isEnemy) speedFactor = 1.0f - speedFactor;
    pushMulti = pushMulti * speedFactor;
    log::trace("pushMulti:{}. IsEnemy:{}. Before x:{}, y:{}", pushMulti, isEnemy, pushDirection.x, pushDirection.y);

    return RE::hkVector4(pushDirection.x * pushMulti, pushDirection.y * pushMulti, 0.0f, 0.0f);
}

// Clear every buffer before player load a save
void ZacOnFrame::CleanBeforeLoad() { 
    colBuffer.Clear();
    PlayerCollision::GetSingleton()->Clear();
    // SpeedRing doesn't really need clear: it will only be used to calculate velocity
    slowTimeData.clear();
    meleeQueue.Clear();
    iFrameCount = 0;
    iFrameSlowCost = 0;
    iFrameTriggerPress = 0;
    iFrameStopBlock = 0;
    parriedProj.Clear();
}
