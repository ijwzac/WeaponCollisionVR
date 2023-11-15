#pragma once

#include <string>

std::string formatNiPoint3(RE::NiPoint3& pos);

bool IsNiPointZero(const RE::NiPoint3& pos);

bool AnyPointZero(const RE::NiPoint3& A, const RE::NiPoint3& B, const RE::NiPoint3& C, const RE::NiPoint3& D);

bool ShouldRotateClockwise(const RE::NiPoint3& colPos, const RE::NiPoint3& enemyPos,
                           const RE::NiPoint3& weaponVelocity);

int64_t RotateFrame(float speed);

bool IsOneHandWeap(RE::Actor*, bool);

bool IsTwoHandWeap(RE::Actor*, bool);

bool IsHandToHand(RE::Actor*, bool);


float CalculatePushDist(bool isEnemy, float speed);

RE::NiMatrix3 ConvertToPlayerSpace(const RE::NiMatrix3& R_weapon_world_space,
                                   const RE::NiMatrix3& R_player_world_space);

RE::NiMatrix3 ConvertToWorldSpace(const RE::NiMatrix3& R_weapon_player_space,
                                  const RE::NiMatrix3& R_player_world_space);

uint32_t GetFullFormID(const uint8_t modIndex, uint32_t formLower);

RE::SpellItem* GetTimeSlowSpell();

class twoNodes {
public:
    RE::NiNode* nodeL;
    RE::NiNode* nodeR;

    twoNodes(RE::NiNode* nodeL, RE::NiNode* nodeR) : nodeL(nodeL), nodeR(nodeR) {}

    bool isEmpty() { return nodeL == nullptr || nodeR == nullptr;
    }
};

// Returns the weapon node if is DwenmerSphere. Returns null if not. Also fills the four positions appropriately
RE::NiNode* HandleDwenmerSphere(RE::Actor* actor, RE::NiPoint3& posWeaponBottomL, RE::NiPoint3& posWeaponBottomR,
                         RE::NiPoint3& posWeaponTopL, RE::NiPoint3& posWeaponTopR);

RE::NiNode* HandleMouthRace(RE::Actor* actor, RE::NiPoint3& posWeaponBottomL, RE::NiPoint3& posWeaponBottomR,
                       RE::NiPoint3& posWeaponTopL, RE::NiPoint3& posWeaponTopR);

twoNodes HandleClawRaces(RE::Actor* actor, RE::NiPoint3& posWeaponBottomL, RE::NiPoint3& posWeaponBottomR,
                       RE::NiPoint3& posWeaponTopL, RE::NiPoint3& posWeaponTopR);

twoNodes HandleClawAndHeadRaces(RE::Actor* actor, RE::NiPoint3& posWeaponBottomL, RE::NiPoint3& posWeaponBottomR,
                            RE::NiPoint3& posWeaponTopL, RE::NiPoint3& posWeaponTopR);

twoNodes HandleFrostSpider(RE::Actor* actor, RE::NiPoint3& posWeaponBottomL, RE::NiPoint3& posWeaponBottomR,
                                RE::NiPoint3& posWeaponTopL, RE::NiPoint3& posWeaponTopR);

twoNodes HandleDwenmerSpider(RE::Actor* actor, RE::NiPoint3& posWeaponBottomL, RE::NiPoint3& posWeaponBottomR,
                    RE::NiPoint3& posWeaponTopL, RE::NiPoint3& posWeaponTopR);

int GetSpecialRace(RE::Actor* actor);

void RecoilEffect(RE::Actor* actor, int strength);


static RE::FormID DwenmerSphereForm = 0x131f2;

static RE::FormID wolfRace = 0x1320A;
static RE::FormID skeever1 = 0x13201;
static RE::FormID skeever2 = 0xc3edf;

static RE::FormID werewolfRace = 0xCDD84;
static RE::FormID raceWereBear = 0x1e17b;  // need to remove higher bits from race
static RE::FormID troll1 = 0x13205;
static RE::FormID troll2 = 0x13206;
static RE::FormID troll3 = 0x117f5;  // need to remove higher bits from race
static RE::FormID troll4 = 0x117f4;  // need to remove higher bits from race
static RE::FormID bear1 = 0x131e7;
static RE::FormID bear2 = 0x131e8;
static RE::FormID bear3 = 0x131e9;
static RE::FormID hagraven = 0x131fb;
static RE::FormID spriggan1 = 0xf3903;
static RE::FormID spriggan2 = 0x13204;
static RE::FormID gargoyle = 0x19d86; // need to remove higher bits
static RE::FormID vampLord = 0x0283a;  // need to remove higher bits

    // forst atronach's hand is not a node. give up

static RE::FormID raceCenturion = 0x131f1;

// cat and spider races
static RE::FormID cat1 = 0x13200;
static RE::FormID cat2 = 0x13202;
static RE::FormID cat3 = 0x0d0b6;  // dlc dark cat // need to remove higher bits from race
static RE::FormID spider = 0x131f8;
static RE::FormID spider2 = 0x4e507;

// dwenmer spider: 131f3
static RE::FormID dwenmerSpider = 0x131f3;




// Deprecated: not working
//RE::BSPointerHandle<RE::Projectile>* LaunchArrow(RE::Projectile* proj, RE::Actor* playerActor,
//                                                        RE::TESObjectREFR* enemy);