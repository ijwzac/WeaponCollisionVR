#include "Settings.h"
#include "OnMeleeHit.h"
#include "Utils.h"

using namespace SKSE;
using namespace SKSE::log;

//void DisableHiggsWeapCol() {
//    if (iFrameCount - iFrameDisableHiggs > 90 || iFrameCount - iFrameDisableHiggs < 0) {
//        iFrameDisableHiggs = iFrameCount;
//        auto papyrusVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();
//        if (papyrusVM && bSwitchHiggsCollision && !isHiggsDisabled) {
//            if (papyrusVM->TypeIsValid("HiggsVR"sv)) {
//                log::debug("HiggsVR is installed. About to disable Higgs weapon collision");
//                RE::BSScript::IFunctionArguments *higgsLeft, *higgsRight;
//                RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
//                /*higgsLeft = RE::MakeFunctionArguments(true);
//                higgsRight = RE::MakeFunctionArguments(false);
//                papyrusVM->DispatchStaticCall("HiggsVR"sv, "DisableWeaponCollision"sv, higgsLeft, callback);
//                papyrusVM->DispatchStaticCall("HiggsVR"sv, "DisableWeaponCollision"sv, higgsRight, callback);*/
//                higgsLeft = RE::MakeFunctionArguments("EnableWeaponCollision"sv, 0.0f);
//                papyrusVM->DispatchStaticCall("HiggsVR"sv, "SetSetting"sv, higgsLeft, callback);
//                isHiggsDisabled = true;
//            } else {
//                log::debug("HiggsVR is NOT installed");
//            }
//        }
//    }
//}
//void EnableHiggsWeapCol() {
//    if ((isHiggsDisabled && iFrameCount - iFrameDisableHiggs >= 180 && iFrameDisableHiggs != 0 &&
//            bSwitchHiggsCollision) ||
//        iFrameCount == 90 && bSwitchHiggsCollision) { 
//        // iFrameCount == 90 means this is the first 90th frame after load. We should re-enable after loading
//        iFrameDisableHiggs = 0;
//        auto papyrusVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();
//        if (papyrusVM) {
//            if (papyrusVM->TypeIsValid("HiggsVR"sv)) {
//                log::debug("HiggsVR is installed. About to re-enable Higgs weapon collision");
//                RE::BSScript::IFunctionArguments *higgsLeft, *higgsRight;
//                RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
//                higgsLeft = RE::MakeFunctionArguments("EnableWeaponCollision"sv, 1.0f);
//                papyrusVM->DispatchStaticCall("HiggsVR"sv, "SetSetting"sv, higgsLeft, callback);
//                isHiggsDisabled = false;
//                // higgsLeft = RE::MakeFunctionArguments(true);
//                // higgsRight = RE::MakeFunctionArguments(false);
//                // papyrusVM->DispatchStaticCall("HiggsVR"sv, "EnableWeaponCollision"sv, higgsLeft, callback);
//                // papyrusVM->DispatchStaticCall("HiggsVR"sv, "EnableWeaponCollision"sv, higgsRight, callback);
//            } else {
//                log::debug("HiggsVR is NOT installed");
//            }
//        }
//    }
//}

std::string formatNiPoint3(RE::NiPoint3& pos) {
    std::ostringstream stream;
    stream << "(" << pos.x << ", " << pos.y << ", " << pos.z << ")";
    return stream.str();
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

bool ShouldRotateClockwise(const RE::NiPoint3& colPos, const RE::NiPoint3& enemyPos,
                           const RE::NiPoint3& weaponVelocity) {
    // Calculate the vector from the collision to the enemy
    RE::NiPoint3 colToEnemy = enemyPos - colPos;

    // Calculate the cross product of the weapon's velocity and the collision-to-enemy vector
    RE::NiPoint3 crossProduct = weaponVelocity.Cross(colToEnemy);

    // Return true if z component is negative (meaning clockwise rotation)
    return crossProduct.z < 0.0f;
}

int64_t RotateFrame(float speed) {
    if (speed > 500) {
        return 20;
    } else if (speed > 400) {
        return 15;
    } else if (speed > 300) {
        return 12;
    } else if (speed > 200) {
        return 10;
    } else if (speed > 100) {
        return 8;
    } else if (speed > 70) {
        return 7;
    } else if (speed > 50) {
        return 6;
    } else {
        return 5;
    }
}

bool IsOneHandWeap(RE::Actor* actor, bool isLeft) { 
    if (auto equip = actor->GetEquippedObject(isLeft)) {
        if (auto equipWeap = equip->As<RE::TESObjectWEAP>(); equipWeap) {
            if (equip->IsArmor()) {
                return false;
            }
            if (equipWeap->IsBow()) {
                return false;
            } else if (equipWeap->IsTwoHandedSword()) {
                return false;
            } else if (equipWeap->IsTwoHandedAxe()) {
                return false;
            } else if (equipWeap->HasKeywordString("WeapTypeWarhammer")) {
                return false;
            } else if (equipWeap->IsStaff()) {
                return false;
            } else {
                return true;
            }
        }
    }
    return false;
}

bool IsTwoHandWeap(RE::Actor* actor, bool isLeft) {
    if (auto equip = actor->GetEquippedObject(isLeft)) {
        if (auto equipWeap = equip->As<RE::TESObjectWEAP>(); equipWeap) {
            if (equip->IsArmor()) {
                return false;
            }
            if (equipWeap->IsBow()) {
                return false;
            } else if (equipWeap->IsTwoHandedSword()) {
                return true;
            } else if (equipWeap->IsTwoHandedAxe()) {
                return true;
            } else if (equipWeap->HasKeywordString("WeapTypeWarhammer")) {
                return true;
            } else if (equipWeap->IsStaff()) {
                return false;
            } else {
                return false;
            }
        }
    }
    return false;
}

bool IsHandToHand(RE::Actor* actor, bool isLeft) {
    if (auto equip = actor->GetEquippedObject(isLeft)) {
        return false;
    } else {
        return true;
    }
}

float CalculatePushDist(bool isEnemy, float speed) {
    float dist;
    if (speed < 20.0f) {
        dist = 100.0f;
    } else if (speed < 40.0f) {
        dist = 70.0f;
    } else if (speed < 60.0f) {
        dist = 50.0f;
    } else if (speed < 80.0f) {
        dist = 35.0f;
    } else if (speed < 100.0f) {
        dist = 15.0f;
    } else {
        dist = 0.0f;
    }
    if (isEnemy) {
        dist = 110.0f - dist;
    }
    return dist;
}

RE::NiMatrix3 ConvertToPlayerSpace(const RE::NiMatrix3& R_weapon_world_space,
                                   const RE::NiMatrix3& R_player_world_space) {
    // Transpose of player's rotation matrix in world space
    RE::NiMatrix3 R_player_world_space_transposed;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R_player_world_space_transposed.entry[i][j] = R_player_world_space.entry[j][i];
        }
    }

    // Multiplying with weapon's rotation matrix in world space
    RE::NiMatrix3 R_weapon_player_space;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R_weapon_player_space.entry[i][j] = 0.0f;
            for (int k = 0; k < 3; ++k) {
                R_weapon_player_space.entry[i][j] +=
                    R_player_world_space_transposed.entry[i][k] * R_weapon_world_space.entry[k][j];
            }
        }
    }

    return R_weapon_player_space;
}

RE::NiMatrix3 ConvertToWorldSpace(const RE::NiMatrix3& R_weapon_player_space,
                                  const RE::NiMatrix3& R_player_world_space) {
    RE::NiMatrix3 R_weapon_world_space;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R_weapon_world_space.entry[i][j] = 0.0f;
            for (int k = 0; k < 3; ++k) {
                R_weapon_world_space.entry[i][j] +=
                    R_player_world_space.entry[i][k] * R_weapon_player_space.entry[k][j];
            }
        }
    }
    return R_weapon_world_space;
}
uint32_t GetBaseFormID(uint32_t formId) { return formId & 0x00FFFFFF; }

uint32_t GetFullFormID(const uint8_t modIndex, uint32_t formLower) { return (modIndex << 24) | formLower; }

// The time slow spell is given by SpeelWheel VR by Shizof. Thanks for the support!
// May return nullptr
RE::SpellItem* GetTimeSlowSpell() { 
    auto handler = RE::TESDataHandler::GetSingleton();
    if (!handler) {
        log::error("GetTimeSlowSpell: failed to get TESDataHandler");
        return nullptr;
    }
    auto spellWheelIndex = handler->GetLoadedModIndex("SpellWheelVR.esp");
    if (!spellWheelIndex.has_value()) {
        log::trace("GetTimeSlowSpell: failed to get spellWheel");
        return nullptr;
    }

    //auto spellWheelMod = handler->LookupLoadedModByName("SpellWheelVR.esp");
    //if (spellWheelMod) {
    //    log::trace("GetTimeSlowSpell: get spellWheel mod");
    //} else {
    //    log::trace("GetTimeSlowSpell: didn't get spellWheel mod");
    //}

    auto vrikMod = handler->LookupLoadedModByName("vrik.esp");
    if (vrikMod) {
        log::trace("GetTimeSlowSpell: get vrik mod");
    } else {
        log::trace("GetTimeSlowSpell: didn't get vrik mod");
    }


    RE::FormID partFormID = 0x000EA5;
    RE::FormID fullFormID = GetFullFormID(spellWheelIndex.value(), partFormID);
    RE::SpellItem* timeSlowSpell = RE::TESForm::LookupByID<RE::SpellItem>(fullFormID); 
    if (!timeSlowSpell) {
        log::error("GetTimeSlowSpell: failed to get timeslow spell");
        return nullptr;
    }
    return timeSlowSpell;
}

RE::NiNode* HandleDwenmerSphere(RE::Actor* actor, RE::NiPoint3& posWeaponBottomL, RE::NiPoint3& posWeaponBottomR,
                         RE::NiPoint3& posWeaponTopL, RE::NiPoint3& posWeaponTopR) {
    if (actor->GetRace() && actor->GetRace()->formID == DwenmerSphereForm) {
    } else {
        return nullptr;
    }

    bool isDwenmerSphere = false;

    const auto actorRoot = netimmerse_cast<RE::BSFadeNode*>(actor->Get3D());
    if (!actorRoot) {
        log::warn("Fail to find actor:{}", actor->GetBaseObject()->GetName());
        return nullptr;
    }

    const auto nodeDwenmerSphereSword = "NPC R BladeLowerGuard"sv;  // right hand blade of dwenmer sphere
    const auto nodeDwenmerSphereBase = "NPC Pelvis"sv;              // base of dwenmer sphere
    const auto weaponDSS = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(nodeDwenmerSphereSword));
    const auto baseDS = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(nodeDwenmerSphereBase));
    if (weaponDSS && baseDS) {  // right hand blade of dwenmer sphere
        isDwenmerSphere = true;
        float reachR(70.0f), handleR(20.0f);
        reachR *= fRangeMulti;
        handleR *= fRangeMulti;
        posWeaponBottomR = weaponDSS->world.translate;

        float x, y, z;
        auto rotation = weaponDSS->world.rotate;
        auto rotation_base = baseDS->world.rotate;
        rotation = ConvertToPlayerSpace(rotation, rotation_base);

        RE::NiPoint3 adjust = RE::NiPoint3(-0.3f, 0.0f, -4.3f);  // -0.3f, 0.0f, -4.3f. A rotation matrix I tested
        rotation = rotation * adjust;

        rotation = ConvertToWorldSpace(rotation, rotation_base);

        auto weaponDirectionR = RE::NiPoint3{rotation.entry[0][1], rotation.entry[1][1], rotation.entry[2][1]};

        posWeaponTopR = posWeaponBottomR + weaponDirectionR * reachR;
        posWeaponBottomR = posWeaponBottomR - weaponDirectionR * handleR;

        log::trace("Is DwenmerSphere. actor:{}", actor->GetBaseObject()->GetName());
    }

    if (isDwenmerSphere) {
        return weaponDSS;
    } else {
        return nullptr;
    }
}

RE::NiNode* HandleMouthRace(RE::Actor* actor, RE::NiPoint3& posWeaponBottomL, RE::NiPoint3& posWeaponBottomR,
                                RE::NiPoint3& posWeaponTopL, RE::NiPoint3& posWeaponTopR) {
    if (actor && actor->GetRace() &&
        (actor->GetRace()->formID == wolfRace || actor->GetRace()->formID == skeever1 ||
         actor->GetRace()->formID == skeever2)) {
    } else {
        return nullptr;
    }
    bool isRace = false;

    const auto actorRoot = netimmerse_cast<RE::BSFadeNode*>(actor->Get3D());
    if (!actorRoot) {
        log::warn("Fail to find actor:{}", actor->GetBaseObject()->GetName());
        return nullptr;
    }

    const auto weaponNodeName = "Canine_RUpperLip"sv; // for wolf
    auto weaponNode = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(weaponNodeName));
    const auto weaponNodeName_Alt1 = "Tongue03"sv; // for skeever Tongue03
    if (!weaponNode) weaponNode = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(weaponNodeName_Alt1));

    if (weaponNode != nullptr) { 
        isRace = true;
        float reachR(10.0f), handleR(10.0f);
        reachR *= fRangeMulti;
        handleR *= fRangeMulti;
        posWeaponBottomR = weaponNode->world.translate;

        auto rotation = weaponNode->world.rotate;

        auto weaponDirectionR = RE::NiPoint3{rotation.entry[0][1], rotation.entry[1][1], rotation.entry[2][1]};

        posWeaponTopR = posWeaponBottomR + weaponDirectionR * reachR;
        posWeaponBottomR = posWeaponBottomR - weaponDirectionR * handleR;

        log::trace("Is wolf. actor:{}", actor->GetBaseObject()->GetName());
    }

    if (isRace) {
        return weaponNode;
    } else {
        return nullptr;
    }
}

twoNodes HandleClawRaces(RE::Actor* actor, RE::NiPoint3& posWeaponBottomL, RE::NiPoint3& posWeaponBottomR,
                       RE::NiPoint3& posWeaponTopL, RE::NiPoint3& posWeaponTopR) {

    if (actor && actor->GetRace() &&
        (actor->GetRace()->formID == werewolfRace || GetBaseFormID(actor->GetRace()->formID) == raceWereBear ||
         actor->GetRace()->formID == troll1 || actor->GetRace()->formID == troll2 ||
         GetBaseFormID(actor->GetRace()->formID) == troll3 || GetBaseFormID(actor->GetRace()->formID) == troll4 ||
         actor->GetRace()->formID == bear1 || actor->GetRace()->formID == bear2 || actor->GetRace()->formID == bear3 ||
         actor->GetRace()->formID == hagraven || actor->GetRace()->formID == spriggan1 ||
         actor->GetRace()->formID == spriggan2 || GetBaseFormID(actor->GetRace()->formID) == gargoyle ||
         GetBaseFormID(actor->GetRace()->formID) == vampLord)) {
    } else {
        return twoNodes(nullptr, nullptr);
    }

    bool isRace = false;

    const auto actorRoot = netimmerse_cast<RE::BSFadeNode*>(actor->Get3D());
    if (!actorRoot) {
        log::warn("Fail to find actor:{}", actor->GetBaseObject()->GetName());
        return twoNodes(nullptr, nullptr);
    }

    const auto weaponNodeNameL = "NPC L Hand [RHnd]"sv;  // yes, werewolves have [RHnd] even for left hand
    const auto weaponNodeNameL_Alt = "NPC L Hand [LHnd]"sv;
    const auto weaponNodeNameR = "NPC R Hand [RHnd]"sv;
    auto weaponNodeL = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(weaponNodeNameL));
    auto weaponNodeR = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(weaponNodeNameR));
    if (!weaponNodeL) weaponNodeL = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(weaponNodeNameL_Alt));

    // for bears
    const auto weaponNodeNameL_Alt2 = "NPC LThumb02"sv;
    const auto weaponNodeNameR_Alt2 = "NPC RThumb02"sv;
    if (!weaponNodeL) weaponNodeL = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(weaponNodeNameL_Alt2));
    if (!weaponNodeR) weaponNodeR = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(weaponNodeNameR_Alt2));



    if (weaponNodeL && weaponNodeR) { 
        isRace = true;

        float reachR(10.0f), handleR(10.0f);
        reachR *= fRangeMulti;
        handleR *= fRangeMulti;
        posWeaponBottomR = weaponNodeR->world.translate;

        auto rotationR = weaponNodeR->world.rotate;

        auto weaponDirectionR = RE::NiPoint3{rotationR.entry[0][1], rotationR.entry[1][1], rotationR.entry[2][1]};

        posWeaponTopR = posWeaponBottomR + weaponDirectionR * reachR;
        posWeaponBottomR = posWeaponBottomR - weaponDirectionR * handleR;

        float reachL(10.0f), handleL(10.0f);
        reachL *= fRangeMulti;
        handleL *= fRangeMulti;
        posWeaponBottomL = weaponNodeL->world.translate;

        auto rotationL = weaponNodeL->world.rotate;

        auto weaponDirectionL = RE::NiPoint3{rotationL.entry[0][1], rotationL.entry[1][1], rotationL.entry[2][1]};

        posWeaponTopL = posWeaponBottomL + weaponDirectionL * reachL;
        posWeaponBottomL = posWeaponBottomL - weaponDirectionL * handleL;


        log::trace("Is werewolf or werebear or trolls. actor:{}", actor->GetBaseObject()->GetName());
    }

    if (isRace) {
        return twoNodes(weaponNodeL, weaponNodeR);
    } else {
        return twoNodes(nullptr, nullptr);
    }
}

twoNodes HandleClawAndHeadRaces(RE::Actor* actor, RE::NiPoint3& posWeaponBottomL, RE::NiPoint3& posWeaponBottomR,
                            RE::NiPoint3& posWeaponTopL, RE::NiPoint3& posWeaponTopR) {
    if (actor->GetRace() &&
        (actor->GetRace()->formID == cat1 || actor->GetRace()->formID == cat2 ||
         GetBaseFormID(actor->GetRace()->formID) == cat3)) {
    } else {
        return twoNodes(nullptr, nullptr);
    }
    bool isRace = false;

    const auto actorRoot = netimmerse_cast<RE::BSFadeNode*>(actor->Get3D());
    if (!actorRoot) {
        log::warn("Fail to find actor:{}", actor->GetBaseObject()->GetName());
        return twoNodes(nullptr, nullptr);
    }

    // we just calculate the right hand and head
    const auto weaponNodeNameL = "Sabrecat_RightHand[RHnd]"sv;   // for cat
    const auto weaponNodeNameR = "Sabrecat_Head[LM01]"sv;
    auto weaponNodeL = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(weaponNodeNameL));
    auto weaponNodeR = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(weaponNodeNameR));
    
    if (weaponNodeL && weaponNodeR) {
        isRace = true;

        float reachR(10.0f), handleR(10.0f);
        reachR *= fRangeMulti;
        handleR *= fRangeMulti;
        posWeaponBottomR = weaponNodeR->world.translate;

        auto rotationR = weaponNodeR->world.rotate;

        auto weaponDirectionR = RE::NiPoint3{rotationR.entry[0][1], rotationR.entry[1][1], rotationR.entry[2][1]};

        posWeaponTopR = posWeaponBottomR + weaponDirectionR * reachR;
        posWeaponBottomR = posWeaponBottomR - weaponDirectionR * handleR;

        float reachL(10.0f), handleL(10.0f);
        reachL *= fRangeMulti;
        handleL *= fRangeMulti;
        posWeaponBottomL = weaponNodeL->world.translate;

        auto rotationL = weaponNodeL->world.rotate;

        auto weaponDirectionL = RE::NiPoint3{rotationL.entry[0][1], rotationL.entry[1][1], rotationL.entry[2][1]};

        posWeaponTopL = posWeaponBottomL + weaponDirectionL * reachL;
        posWeaponBottomL = posWeaponBottomL - weaponDirectionL * handleL;

        log::trace("Is a cat. actor:{}", actor->GetBaseObject()->GetName());
    }

    if (isRace) {
        return twoNodes(weaponNodeL, weaponNodeR);
    } else {
        return twoNodes(nullptr, nullptr);
    }
}

twoNodes HandleFrostSpider(RE::Actor* actor, RE::NiPoint3& posWeaponBottomL, RE::NiPoint3& posWeaponBottomR,
                                RE::NiPoint3& posWeaponTopL, RE::NiPoint3& posWeaponTopR) {
    if (actor->GetRace() &&
        (actor->GetRace()->formID == spider || actor->GetRace()->formID == spider2)) {
    } else {
        return twoNodes(nullptr, nullptr);
    }
    bool isRace = false;

    const auto actorRoot = netimmerse_cast<RE::BSFadeNode*>(actor->Get3D());
    if (!actorRoot) {
        log::warn("Fail to find actor:{}", actor->GetBaseObject()->GetName());
        return twoNodes(nullptr, nullptr);
    }

    
    const auto weaponNodeNameL = "FangL[02]"sv;  // We only calculate its mouth and right claw, since it prefers right claw
    const auto weaponNodeNameR = "ArmRClaw"sv;  // For spider
    auto weaponNodeL = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(weaponNodeNameL));
    auto weaponNodeR = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(weaponNodeNameR));static RE::FormID cat1 = 0x13200;
   
    if (weaponNodeL && weaponNodeR ) {
        isRace = true;

        float reachR(15.0f), handleR(15.0f);
        float reachL(10.0f), handleL(10.0f);
        reachR *= fRangeMulti;
        handleR *= fRangeMulti;
        reachL *= fRangeMulti;
        handleL *= fRangeMulti;
        if (actor->GetRace()->formID == spider2) {
            // giant spider
            reachR *= 2.5;
            handleR *= 2.5;
            reachL *= 3;
            handleL *= 3;
        }
        posWeaponBottomR = weaponNodeR->world.translate;

        auto rotationR = weaponNodeR->world.rotate;

        auto weaponDirectionR = RE::NiPoint3{rotationR.entry[0][1], rotationR.entry[1][1], rotationR.entry[2][1]};

        posWeaponTopR = posWeaponBottomR + weaponDirectionR * reachR;
        posWeaponBottomR = posWeaponBottomR - weaponDirectionR * handleR;

        posWeaponBottomL = weaponNodeL->world.translate;

        auto rotationL = weaponNodeL->world.rotate;

        auto weaponDirectionL = RE::NiPoint3{rotationL.entry[0][1], rotationL.entry[1][1], rotationL.entry[2][1]};

        posWeaponTopL = posWeaponBottomL + weaponDirectionL * reachL;
        posWeaponBottomL = posWeaponBottomL - weaponDirectionL * handleL;

        log::trace("Is a frostbite spider. actor:{}", actor->GetBaseObject()->GetName());
    }

    if (isRace) {
        return twoNodes(weaponNodeL, weaponNodeR);
    } else {
        return twoNodes(nullptr, nullptr);
    }
}

twoNodes HandleDwenmerSpider(RE::Actor* actor, RE::NiPoint3& posWeaponBottomL, RE::NiPoint3& posWeaponBottomR,
                    RE::NiPoint3& posWeaponTopL, RE::NiPoint3& posWeaponTopR) {
    if (actor->GetRace() && (actor->GetRace()->formID == dwenmerSpider)) {
    } else {
        return twoNodes(nullptr, nullptr);
    }
    bool isRace = false;

    const auto actorRoot = netimmerse_cast<RE::BSFadeNode*>(actor->Get3D());
    if (!actorRoot) {
        log::warn("Fail to find actor:{}", actor->GetBaseObject()->GetName());
        return twoNodes(nullptr, nullptr);
    }

    const auto weaponNodeNameL = "DwarvenSpiderRLeg03"sv;  // we just calculate the right leg and pincher
    const auto weaponNodeNameR = "DwarvenSpiderRightPincher1"sv;
    const auto weaponNodeL = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(weaponNodeNameL));
    const auto weaponNodeR = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(weaponNodeNameR));


    if (weaponNodeL && weaponNodeR) {
        isRace = true;

        float reachR(5.0f), handleR(5.0f);
        reachR *= fRangeMulti;
        handleR *= fRangeMulti;
        posWeaponBottomR = weaponNodeR->world.translate;

        auto rotationR = weaponNodeR->world.rotate;

        auto weaponDirectionR = RE::NiPoint3{rotationR.entry[0][1], rotationR.entry[1][1], rotationR.entry[2][1]};

        posWeaponTopR = posWeaponBottomR + weaponDirectionR * reachR;
        posWeaponBottomR = posWeaponBottomR - weaponDirectionR * handleR;

        float reachL(5.0f), handleL(5.0f);
        reachL *= fRangeMulti;
        handleL *= fRangeMulti;
        posWeaponBottomL = weaponNodeL->world.translate;

        auto rotationL = weaponNodeL->world.rotate;

        auto weaponDirectionL = RE::NiPoint3{rotationL.entry[0][1], rotationL.entry[1][1], rotationL.entry[2][1]};

        posWeaponTopL = posWeaponBottomL + weaponDirectionL * reachL;
        posWeaponBottomL = posWeaponBottomL - weaponDirectionL * handleL;

        log::trace("Is a dwenmer spider. actor:{}", actor->GetBaseObject()->GetName());
    }

    if (isRace) {
        return twoNodes(weaponNodeL, weaponNodeR);
    } else {
        return twoNodes(nullptr, nullptr);
    }
}

int GetSpecialRace(RE::Actor* actor) {

    if (!actor || !actor->GetRace()) return -1;
    if (actor->GetRace()->formID == DwenmerSphereForm) {
        return 1;
    }
    if (actor->GetRace()->formID == wolfRace || actor->GetRace()->formID == skeever1 ||
        actor->GetRace()->formID == skeever2) {
        return 2;
    }
    if (actor->GetRace()->formID == werewolfRace || GetBaseFormID(actor->GetRace()->formID) == raceWereBear ||
        actor->GetRace()->formID == troll1 || actor->GetRace()->formID == troll2 ||
        GetBaseFormID(actor->GetRace()->formID) == troll3 ||
        GetBaseFormID(actor->GetRace()->formID) == troll4 || actor->GetRace()->formID == bear1 || 
        actor->GetRace()->formID == bear2 || actor->GetRace()->formID == bear3 ||
        actor->GetRace()->formID == hagraven || actor->GetRace()->formID == spriggan1 ||
        actor->GetRace()->formID == spriggan2 || GetBaseFormID(actor->GetRace()->formID) == gargoyle ||
        GetBaseFormID(actor->GetRace()->formID) == vampLord) {
        return 3;
    }
    if (actor->GetRace()->formID == raceCenturion) {
        return 4;
    }
    if (actor->GetRace()->formID == cat1 || actor->GetRace()->formID == cat2 ||
        GetBaseFormID(actor->GetRace()->formID) == cat3) {
        return 5;
    }
    if (actor->GetRace()->formID == dwenmerSpider) {
        return 6;
    }

    return 0; // human
}

// strength can only be 1 or 2. 1 is attack stop or recoil, 2 is large recoil
void RecoilEffect(RE::Actor* actor, int strength) {
    if (!actor) return;
    // Animation names below are checked in Idle Animations in CK for each race, see Anim Even on the right side
    bool canRagdoll = false;
    switch (GetSpecialRace(actor)) {
        case 1:  // dwenmer sphere
            actor->NotifyAnimationGraph("recoilStart");
            if (strength == 2) {
                actor->NotifyAnimationGraph("recoilLargeStart");
                canRagdoll = false; // After sphere is ragdolled, it will have bug and can't be hit
            }
            break;
        case 5:  // cats

        case 6:  // dwenmer spider
            
        case 2:  // races that only use mouth as weapon: wolf, skeevers
            actor->NotifyAnimationGraph("recoilStart");
            if (strength == 2) {
                actor->NotifyAnimationGraph("recoilLargeStart");
                canRagdoll = true;
            }
            break;
        case 3:  // races that use claws: werewolf or werebear or trolls or bears or hagraven
            if (actor->GetRace()->formID == werewolfRace || GetBaseFormID(actor->GetRace()->formID) == raceWereBear) {
                actor->NotifyAnimationGraph("StaggerSmallRight"); 
                if (strength == 2) {
                    actor->NotifyAnimationGraph("StaggerBackMedium");
                    canRagdoll = true;
                }
            } else if (actor->GetRace()->formID == troll1 || actor->GetRace()->formID == troll2 ||
                       GetBaseFormID(actor->GetRace()->formID) == troll3 ||
                       GetBaseFormID(actor->GetRace()->formID) == troll4) {
                actor->NotifyAnimationGraph("recoilStart");
                if (strength == 2) {
                    actor->NotifyAnimationGraph("recoilLargeStart");
                    canRagdoll = true;
                }
            } else if (actor->GetRace()->formID == bear1 || actor->GetRace()->formID == bear2 ||
                       actor->GetRace()->formID == bear3) {
                actor->NotifyAnimationGraph("recoilStart");
                if (strength == 2) {
                    actor->NotifyAnimationGraph("recoilLargeStartBear");
                    canRagdoll = true;
                }
            } else if (GetBaseFormID(actor->GetRace()->formID) == gargoyle ||
                       GetBaseFormID(actor->GetRace()->formID) == vampLord) {
                actor->NotifyAnimationGraph("recoilStart");
                if (strength == 2) {
                    actor->NotifyAnimationGraph("staggerStart");
                    canRagdoll = false; // ragdoll has severe bug on vampire lord, and will break AI of gargoyle
                }
            } else if (actor->GetRace()->formID == hagraven || actor->GetRace()->formID == spriggan1 ||
                       actor->GetRace()->formID == spriggan2) {
                actor->NotifyAnimationGraph("recoilStart");
                if (strength == 2) {
                    actor->NotifyAnimationGraph("staggerStart");
                    canRagdoll = true;
                }
            }
            
            break;
        case 0: // human
            actor->NotifyAnimationGraph("recoilStop");
            actor->NotifyAnimationGraph("AttackStop");
            if (strength == 2) {
                actor->NotifyAnimationGraph("recoilLargeStart");
                canRagdoll = true;
            }
            break;
        case -1:
            log::error("Fail to get special race because actor or race is null");
            return;
        default:
            break;
            return;
    }


    if (canRagdoll && 
        actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth) <
        fEnemyHealthRagdollThresPer *
                                         actor->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kHealth)) {
        // If PLANCK is installed, we can just play the ragdoll animation
        if (bPlanck) {
            actor->NotifyAnimationGraph("RagdollInstant");
        } else {
            // If PLANCK is not installed, playing the animation makes them lie on ground
            //actor->NotifyAnimationGraph("Ragdoll");
            /*auto papyrusVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (papyrusVM) {
                log::trace("Calling papyrus");
                if (papyrusVM->TypeIsValid("ObjectReference"sv)) {
                    log::trace("ObjectReference is installed");
                    auto playerAA = RE::PlayerCharacter::GetSingleton();
                    if (playerAA) {
                        auto playerRef = static_cast<RE::TESObjectREFR*>(playerAA);
                        if (playerRef) {
                            RE::BSScript::IFunctionArguments* hapticArgs = RE::MakeFunctionArguments(
                                true, (RE::TESObjectREFR*)playerRef, (RE::Actor*) actor, (float) 2.0);
                            
                        }
                    }
                    
                }
            } */
        }
    }
}

RE::Projectile::LaunchData::~LaunchData() = default;

// Deprecated: Not working
// Many code learnt from https://github.com/TESRSkywind/SkywindProjectiles
//RE::BSPointerHandle<RE::Projectile>* LaunchArrow(RE::Projectile* proj, RE::Actor* playerActor,
//                                                RE::TESObjectREFR* enemy) { 
//    RE::Projectile::PROJECTILE_RUNTIME_DATA& projRuntime = proj->GetProjectileRuntimeData();
//    RE::Projectile::LaunchData data = RE::Projectile::LaunchData();
//    data.origin = proj->GetPosition();
//    data.contactNormal = RE::NiPoint3(0, 0, 0);
//    auto ammo = projRuntime.ammoSource;
//    if (!ammo) {
//        log::error("Projectile arrow without ammo. Form:{}", (std::uint32_t)proj->formID);
//        return nullptr;
//    }
//    data.projectileBase = ammo->data.projectile;
//    if (!ammo->data.projectile) {
//        log::warn("Projectile arrow ammo with null projectile ptr. Form:{}", (std::uint32_t)proj->formID);
//    }
//    data.shooter = playerActor;
//    data.combatController = playerActor->GetActorRuntimeData().combatController;
//    if (!data.combatController) {
//        // This branch is chosen
//        log::warn("Projectile arrow ammo with null combatController ptr. Form:{}", (std::uint32_t)proj->formID);
//    }
//    data.weaponSource = projRuntime.weaponSource;
//    data.ammoSource = ammo;
//    data.angleZ = playerActor->GetAngleZ();
//    data.angleX = playerActor->GetAngleX();
//    data.unk50 = nullptr;
//    data.desiredTarget = enemy;
//    data.unk60 = 0.0f;
//    data.unk64 = 0.0f;
//    data.parentCell = playerActor->GetParentCell();
//    data.spell = nullptr;
//    data.castingSource = RE::MagicSystem::CastingSource::kOther;
//    data.unk7C = 0; // TODO: see if this number is OK
//    data.enchantItem = nullptr;
//    data.poison = nullptr;
//    data.area = 0; // TODO: see if this number is OK
//    data.power = projRuntime.power;
//    data.scale = projRuntime.scale;
//    data.alwaysHit = false; // TODO: see if this number is OK
//    data.noDamageOutsideCombat = false;  // TODO: see if this number is OK
//    data.autoAim = false;
//    data.unk9F = false;
//    data.useOrigin = false;
//    data.deferInitialization = false;
//    data.forceConeOfFire = false;
//
//    RE::BSPointerHandle<RE::Projectile>* handle = new RE::BSPointerHandle<RE::Projectile>;
//    //RE::BSPointerHandle<RE::Projectile> handle = RE::BSPointerHandle<RE::Projectile>(proj);
//    log::trace("Launching new arrow at enemy:{}", enemy->GetDisplayFullName());
//    return RE::Projectile::Launch(handle, data);
//
//}