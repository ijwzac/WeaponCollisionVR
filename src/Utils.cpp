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