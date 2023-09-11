#pragma once


bool IsNiPointZero(const RE::NiPoint3& pos);

bool AnyPointZero(const RE::NiPoint3& A, const RE::NiPoint3& B, const RE::NiPoint3& C, const RE::NiPoint3& D);

bool ShouldRotateClockwise(const RE::NiPoint3& colPos, const RE::NiPoint3& enemyPos,
                           const RE::NiPoint3& weaponVelocity);

int64_t RotateFrame(float speed);