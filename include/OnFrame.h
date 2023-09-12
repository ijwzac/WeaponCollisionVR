// We hook into the Main Update() function, which fires every frame
//
// On each frame, get the position and rotation of player's weapon, and nearby enemy actors' weapons
// Then detect if player's weapon collides with enemy's weapon and check some other conditions,
//		call collision functions, and then update some data struct
//
// The data here will be used by OnMeleeHit.cpp, to change or even nullify hit events that happen shortly after the
// collision
#pragma once

#include <RE/Skyrim.h>
#include "Settings.h"
#include "OnMeleeHit.h"

using namespace SKSE;
using namespace SKSE::log;

namespace ZacOnFrame {

    void InstallFrameHook();
    void OnFrameUpdate();
    void CollisionDetection();
    bool FrameGetWeaponPos(RE::Actor*, RE::NiPoint3&, RE::NiPoint3&, RE::NiPoint3&, RE::NiPoint3&, bool);
    bool IsNiPointZero(const RE::NiPoint3&);
    void CollisionEffect(RE::Actor*, RE::Actor*, RE::NiPoint3 contactPos, bool, bool);

    

    // Stores the original OnFrame function, and call it no matter what later, so we don't break game's functionality
    static REL::Relocation<decltype(OnFrameUpdate)> _OnFrame;

    // Describes a collision between the player and an enemy
    class Collision {
    public:
        RE::Actor* enemy;
        int64_t iFrameCollision;
        int affectEnemyOnHit = 0; // If 1, the next OnHit event from enemy is nullified, and sets this to 0
        int affectPlayerOnHit = 0;
        RE::hkVector4 hkv; // the velocity used to push enemy away
        RE::NiPoint3 enemyOriPos; // the original position of enemy at collision, used to prevent enemy from being push too far
        float enemyOriAngleZ;
        int angleSetCount = 0;
        bool enemyIsRotClockwise;
        int64_t enemyRotFrame; // how many frames does they rotate

        // We must have a default constructor, to avoid non-determined behavior
        Collision() : enemy(nullptr), iFrameCollision(0) {}
        // Parameterized constructor
        Collision(RE::Actor* e, int64_t frame, RE::NiPoint3 p, RE::hkVector4 h, float z, bool cl, int64_t ro)
            : enemy(e),
              iFrameCollision(frame),
              affectEnemyOnHit(1),
              affectPlayerOnHit(1),
              enemyOriPos(p),
              hkv(h),
              enemyOriAngleZ(z),
              enemyIsRotClockwise(cl), enemyRotFrame(ro) {}
       
        // Accessors for the class members (could be useful in comparisons)
        RE::Actor* getEnemy() const { return enemy; }
        int64_t getFrame() const { return iFrameCollision; }
        void setValues(RE::Actor* e, int64_t frame, RE::NiPoint3 p, RE::hkVector4 h, float z, bool cl, int64_t ro) {
            enemy = e;
            iFrameCollision = frame;
            enemyOriPos = p;
            hkv = h;
            affectEnemyOnHit = 1;
            affectPlayerOnHit = 1;
            enemyOriAngleZ = z;
            angleSetCount = 0;
            enemyIsRotClockwise = cl;
            enemyRotFrame = ro;
        }
        void setValues(Collision& col) {
            enemy = col.enemy;
            iFrameCollision = col.iFrameCollision;
            enemyOriPos = col.enemyOriPos;
            hkv = col.hkv;
            affectEnemyOnHit = 1;
            affectPlayerOnHit = 1;
            enemyOriAngleZ = col.enemyOriAngleZ;
            angleSetCount = 0;
            enemyIsRotClockwise = col.enemyIsRotClockwise;
            enemyRotFrame = col.enemyRotFrame;
        }

        // When there was a collision with the same enemy happened recently, stop calculating collision for this enemy
        bool shouldIgnoreCollision() {
            if (iFrameCount - iFrameCollision < collisionIgnoreDur) {
                return true;
            } else {
                return false;
            }
        }

        // Should be called during OnMeleeHit of enemy. If return true, their hit should be nullified by caller
        // Conditions: the frame is very close, or (the frame is relatively close and affectEnemyOnHit is 1)
        bool shouldNullifyEnemyCurretHit() {
            bool nullify = false;
            auto diff = iFrameCount - iFrameCollision;
            if (diff > 0 && diff < collisionEffectDurEnemyShort) {
                nullify = true;
            }
            if (diff > 0 && diff < collisionEffectDurEnemyLong) {
                if (affectEnemyOnHit == 1) {
                    nullify = true;
                }
            }
            if (affectEnemyOnHit == 1) {
                affectEnemyOnHit = 0;
            }
            return nullify;
        }

        void ChangeAngle() { 
            if (angleSetCount < enemyRotFrame) {
                if (enemyIsRotClockwise) {
                    enemy->SetRotationZ(enemyOriAngleZ + angleSetCount * fEnemyRotStep);
                } else {
                    enemy->SetRotationZ(enemyOriAngleZ - angleSetCount * fEnemyRotStep);
                }
                angleSetCount++;
            }
        }

        // For several frames, change the velocity of enemy, to push them away
        void ChangeVelocity() {
        
            if (hkv.SqrLength3() < 10.0f) {
                return;
            }

            // If enemy is already far enough, set hkv to zero so they won't be pushed farther
            if (enemy->GetPosition().GetDistance(enemyOriPos) > fEnemyPushMaxDist) {
                log::trace("Setting hkv to 0");
                hkv = hkv * 0.0f;
            }

            log::trace("Normal push. hkv length:{}", hkv.SqrLength3());

            if (enemy && enemy->GetCharController()) {
                RE::hkVector4 tmp;
                enemy->GetCharController()->GetLinearVelocityImpl(tmp);
                //tmp = tmp + hkv; // new speed considers old speed
                tmp = hkv;  // new speed doesn't consider old speed
                enemy->GetCharController()->SetLinearVelocityImpl(tmp);
            }

            
            return;
        }
    };

    // A ring buffer to store recent collisions, and provides functions to check if there is a recent collision
    // Each enemy only has its latest collision record in this ring. 
    class CollisionRing {
        std::vector<Collision> buffer;
        std::size_t capacity; // how many collisions can be stored. Oldest one will be replaced by new one
    public:
        CollisionRing(std::size_t cap) : buffer(cap), capacity(cap) {}

        void PushCopy(Collision col) { 
            log::trace("In CollisionRing::PushCopy");
            // If the enemy already has a Collision in the buffer, overwrite it
            for (size_t i = 0; i < capacity; i++) {
                if (buffer[i].getEnemy() == col.getEnemy()) {
                    buffer[i].setValues(col);
                    return;
                }
            }

            // New enemy. Let's find a slot for it

            // If buffer is not full, find any empty slot
            for (size_t i = 0; i < capacity; i++) {
                if (buffer[i].getEnemy() == nullptr) {
                    buffer[i].setValues(col);
                    return;
                }
            }

            // OK, buffer is full, find the actor whose collision is oldest
            size_t index_old = 0;
            Collision* oldest = nullptr;
            for (size_t i = 0; i < capacity; i++) {
                if (oldest == nullptr || (oldest != nullptr && buffer[i].getFrame() < oldest->getFrame())) {
                    oldest = &buffer[i];
                }
            }
            oldest->setValues(col);
        }

        // may return null
        Collision* GetThisEnemyLatestCollision(RE::Actor* enemy) { 
            log::trace("In GetThisEnemyLatestCollision");
            for (size_t i = 0; i < capacity; i++) {
                if (buffer[i].getEnemy() == enemy) {
                    return &buffer[i];
                }
            }
            return nullptr;
        }
    };

    extern CollisionRing colBuffer;

    class WeaponPos { // this class is just used to store player's weapon pos and calculate speed
    public:
        RE::NiPoint3 bottom;
        RE::NiPoint3 top;
        WeaponPos() = default;
        WeaponPos(RE::NiPoint3 b, RE::NiPoint3 t) : bottom(b), top(t) {}
    };

    class SpeedRing {
        std::vector<WeaponPos> bufferL;
        std::vector<WeaponPos> bufferR;
        std::size_t capacity; // how many latest frames are stored
        std::size_t indexCurrentL;
        std::size_t indexCurrentR;
    public:
        SpeedRing(std::size_t cap) : bufferL(cap), bufferR(cap), capacity(cap), indexCurrentR(0), indexCurrentL(0) {}
        
        void Push(WeaponPos p, bool isLeft) { 
            if (isLeft) {
                bufferL[indexCurrentL] = p;
                indexCurrentL = (indexCurrentL + 1) % capacity;
            } else {
                bufferR[indexCurrentR] = p;
                indexCurrentR = (indexCurrentR + 1) % capacity;
            }
        }

        // Thanks ChatGPT for generating this function
        RE::NiPoint3 GetVelocity(std::size_t N, bool isLeft) const {
            if (N == 0 || N > capacity) {
                // Return zero velocity or handle error
                return RE::NiPoint3(0.0f, 0.0f, 0.0f);
            }

            std::size_t currentIdx = isLeft ? indexCurrentL : indexCurrentR;
            const std::vector<WeaponPos>& buffer = isLeft ? bufferL : bufferR;

            // Get the start and end positions
            RE::NiPoint3 startPosBottom = buffer[(currentIdx - N + capacity) % capacity].bottom;
            RE::NiPoint3 endPosBottom = buffer[(currentIdx - 1 + capacity) % capacity].bottom;
            RE::NiPoint3 startPosTop = buffer[(currentIdx - N + capacity) % capacity].top;
            RE::NiPoint3 endPosTop = buffer[(currentIdx - 1 + capacity) % capacity].top;

            // Calculate velocities
            RE::NiPoint3 velocityBottom = (endPosBottom - startPosBottom) / static_cast<float>(N);
            RE::NiPoint3 velocityTop = (endPosTop - startPosTop) / static_cast<float>(N);

            // Return the larger velocity based on magnitude
            return (velocityBottom.Length() > velocityTop.Length()) ? velocityBottom : velocityTop;
        }
    };
    extern SpeedRing speedBuf;

    RE::hkVector4 CalculatePushVector(RE::NiPoint3 sourcePos, RE::NiPoint3 targetPos, bool isEnemy);


}

