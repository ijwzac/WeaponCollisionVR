#include "Settings.h"
#include "OnMeleeHit.h"
#include "Utils.h"

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