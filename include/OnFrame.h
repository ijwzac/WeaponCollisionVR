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
    void CollisionEffect(RE::Actor*, RE::Actor*, RE::NiPoint3 contactPos, bool);

    

    // Stores the original OnFrame function, and call it no matter what later, so we don't break game's functionality
    static REL::Relocation<decltype(OnFrameUpdate)> _OnFrame;

    // Describes a collision between the player and an enemy
    class Collision {
        RE::Actor* enemy;
        int64_t iFrameCollision;
        int affectEnemyOnHit = 0; // If 1, the next OnHit event from enemy is nullified, and sets this to 0
        int affectPlayerOnHit = 0;
        RE::hkVector4 hkv;
    public:
        // We must have a default constructor, to avoid non-determined behavior
        Collision() : enemy(nullptr), iFrameCollision(0) {}
        // Parameterized constructor
        Collision(RE::Actor* e, int64_t frame, RE::hkVector4 h)
            : enemy(e), iFrameCollision(frame), affectEnemyOnHit(1), affectPlayerOnHit(1), hkv(h) {}
       
        // Accessors for the class members (could be useful in comparisons)
        RE::Actor* getEnemy() const { return enemy; }
        int64_t getFrame() const { return iFrameCollision; }
        void setEnemy(RE::Actor* newEnemy) { enemy = newEnemy; }
        void setFrame(int64_t newFrame) { iFrameCollision = newFrame; }

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

        // For several frames, change the velocity of enemy, to push them away
        void ChangeVelocity() {
            if (enemy && enemy->GetCharController()) {
                log::trace("Changing velocity");
                RE::hkVector4 tmp;
                enemy->GetCharController()->GetLinearVelocityImpl(tmp);
                //tmp = tmp + hkv;
                tmp = hkv;
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
                    buffer[i].setFrame(col.getFrame());
                    return;
                }
            }

            // New enemy. Let's find a slot for it

            // If buffer is not full, find any empty slot
            for (size_t i = 0; i < capacity; i++) {
                if (buffer[i].getEnemy() == nullptr) {
                    buffer[i] = col;
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
            oldest->setEnemy(col.getEnemy());
            oldest->setFrame(col.getFrame());
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

    RE::hkVector4 CalculatePushVector(RE::NiPoint3 sourcePos, RE::NiPoint3 targetPos, bool isEnemy);
}

