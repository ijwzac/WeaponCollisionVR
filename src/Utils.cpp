#include "Settings.h"
#include "OnMeleeHit.h"
#include "Utils.h"

// Returns a reference to the one and only instance of EventProcessor :)
//
// Note: this is returned as a & reference. When you need this as a pointer, you'll want to use & (see below)
EventProcessor& EventProcessor::GetSingleton() {
    static EventProcessor singleton;
    return singleton;
}

// Code from series: https://www.youtube.com/c/skyrimscripting
// Log information about Hit events that happen in the game
RE::BSEventNotifyControl EventProcessor::ProcessEvent(const RE::TESHitEvent* event,
                                                      RE::BSTEventSource<RE::TESHitEvent>*) {
    if (!event->cause.get()) {
        logger::warn("Debug Hit: no cause");
        return RE::BSEventNotifyControl::kContinue;
    } else if (!event->target.get()) {
        logger::warn("Debug Hit: no target");
        return RE::BSEventNotifyControl::kContinue;
    }
    if (!event->cause.get()->GetBaseObject()) {
        logger::warn("Debug Hit: no cause base object");
        return RE::BSEventNotifyControl::kContinue;
    } else if (!event->target.get()->GetBaseObject()) {
        logger::warn("Debug Hit: no target base object");
        return RE::BSEventNotifyControl::kContinue;
    }
    logger::info("{} hit {}. Projectile Form:{:x}. Source Form:{:x}",
                    event->cause.get()->GetBaseObject()->GetName(), event->target.get()->GetBaseObject()->GetName(),
                    event->projectile, event->source);
    return RE::BSEventNotifyControl::kContinue;
}
RE::BSEventNotifyControl EventProcessor::ProcessEvent(RE::InputEvent* const* eventPtr,
                                                      RE::BSTEventSource<RE::InputEvent*>*) {

    // The code below return is for debug: show the range of player's weapon and enemy's weapons
    return RE::BSEventNotifyControl::kContinue;
    // logger::info("InputEvent fired");
    if (!eventPtr) return RE::BSEventNotifyControl::kContinue;

    // logger::info("eventPtr checked");

    auto* event = *eventPtr;
    if (!event) return RE::BSEventNotifyControl::kContinue;

    // logger::info("event checked");

    if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton) {
        auto* buttonEvent = event->AsButtonEvent();
        auto dxScanCode = buttonEvent->GetIDCode();
        // logger::info("===Pressed key {}", dxScanCode);
    }

    if (globalInputCounter++ % 4 == 0) {
        // Get player
        auto playerAA = RE::PlayerCharacter::GetSingleton();
        if (playerAA) {
            auto playerActor = static_cast<RE::Actor*>(playerAA);

            // Show spark at the root and tip of player's weapon

            //logger::info("Spark on player weapon");
            //const auto nodeName = "WEAPON"sv;
            const auto nodeName = "SHIELD"sv;
            if (playerActor) {
                SKSE::GetTaskInterface()->AddTask(
                    [playerActor, nodeName]() { OnMeleeHit::debug_show_weapon_collide(playerActor, nodeName); });
            }

            // logger::info("About to cast player to ref");
            auto playerRef = static_cast<RE::TESObjectREFR*>(playerAA);
            if (playerRef) {
                // logger::info("Cast success");
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
                }
                //logger::info("Found {} actors within {} radius", vNearbyObj.size(), fDetectEnemy);
                for (auto* obj : vNearbyObj) {
                    if (obj != playerRef) {
                        // logger::info("About to cast obj to actor");
                        if (auto actorNPC = static_cast<RE::Actor*>(obj); actorNPC) {
                            logger::info("Cast success. NPC name:{}", actorNPC->GetBaseObject()->GetName());
                            SKSE::GetTaskInterface()->AddTask([actorNPC, nodeName]() {
                                OnMeleeHit::debug_show_weapon_collide(actorNPC, nodeName);
                            });
                        }
                    } else {
                        //logger::info("Yes, player is in vNearbyObj");
                    }
                }
            }
        }
    }

    return RE::BSEventNotifyControl::kContinue;
}

bool IsNiPointZero(const RE::NiPoint3& pos) {
    if (pos.x == 0.0f && pos.y == 0.0f && pos.z == 0.0f) {
        return true;
    } else {
        return false;
    }
}

bool AnyPointZero(const RE::NiPoint3& A, const RE::NiPoint3& B, const RE::NiPoint3& C, const RE::NiPoint3& D) { 
    if (IsNiPointZero(A) || IsNiPointZero(B) || IsNiPointZero(C) || IsNiPointZero(D)) {
        return true;
    }
    return false;
}