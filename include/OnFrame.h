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

extern bool bDisableSpark;

namespace ZacOnFrame {

    void InstallFrameHook();
    void OnFrameUpdate();
    void CollisionDetection();
    bool FrameGetWeaponPos(RE::Actor*, RE::NiPoint3&, RE::NiPoint3&, RE::NiPoint3&, RE::NiPoint3&, bool);
    bool FrameGetWeaponFixedPos(RE::Actor*, RE::NiPoint3&, RE::NiPoint3&, RE::NiPoint3&, RE::NiPoint3&);
    bool IsNiPointZero(const RE::NiPoint3&);
    void CollisionEffect(RE::Actor*, RE::Actor*, RE::NiPoint3 contactPos, bool, bool);
    RE::hkVector4 CalculatePushVector(RE::NiPoint3 sourcePos, RE::NiPoint3 targetPos, bool isEnemy, float speed);
    float CalculatePlayerPushDist(float speed);
    void TimeSlowEffect(RE::Actor*, int64_t);
    void StopTimeSlowEffect(RE::Actor*);
    void CleanBeforeLoad();
    

    // Stores the original OnFrame function, and call it no matter what later, so we don't break game's functionality
    static REL::Relocation<decltype(OnFrameUpdate)> _OnFrame;

    // Describes a collision between the player and an enemy
    class Collision {
    public:
        RE::Actor* enemy;
        int64_t iFrameCollision;
        int affectEnemyOnHit = 0; // If 1, the next OnHit event from enemy is nullified, and sets this to 0
        RE::hkVector4 hkv; // the velocity used to push enemy away
        RE::NiPoint3 enemyOriPos; // the original position of enemy at collision, used to prevent enemy from being push too far
        float pushEnemyMaxDist; // the max distance to push enemy. We check both this and fEnemyPushMaxDist
        float contactToEnemyBottom;  // the distance between collision point and enemy weapon bottom, used to spawn spark for several frames
        bool isEnemyLeft; // if the collision happens on enemy's left weapon
        float enemyOriAngleZ;
        int angleSetCount = 0;
        bool enemyIsRotClockwise;
        int64_t enemyRotFrame; // how many frames does they rotate

        // We must have a default constructor, to avoid non-determined behavior
        Collision() : enemy(nullptr), iFrameCollision(-1) {}
        // Parameterized constructor
        Collision(RE::Actor* e, int64_t frame, RE::NiPoint3 pE, float pushEnemyMaxDist, float contactToEnemyBottom,
                  bool isLeft,
                  RE::hkVector4 h, float z, bool cl,
                  int64_t ro)
            : enemy(e),
              iFrameCollision(frame),
              affectEnemyOnHit(1),
              enemyOriPos(pE),
              pushEnemyMaxDist(pushEnemyMaxDist),
              contactToEnemyBottom(contactToEnemyBottom),
              isEnemyLeft(isLeft),
              hkv(h),
              enemyOriAngleZ(z),
              enemyIsRotClockwise(cl), enemyRotFrame(ro) {}
       
        RE::Actor* getEnemy() const { return enemy; }
        int64_t getFrame() const { return iFrameCollision; }
        void setValues(Collision& col) {
            enemy = col.enemy;
            iFrameCollision = col.iFrameCollision;
            enemyOriPos = col.enemyOriPos;
            pushEnemyMaxDist = col.pushEnemyMaxDist;
            contactToEnemyBottom = col.contactToEnemyBottom;
            isEnemyLeft = col.isEnemyLeft;
            hkv = col.hkv;
            affectEnemyOnHit = 1;
            enemyOriAngleZ = col.enemyOriAngleZ;
            angleSetCount = 0;
            enemyIsRotClockwise = col.enemyIsRotClockwise;
            enemyRotFrame = col.enemyRotFrame;
        }

        // When there was a collision with the same enemy happened recently, stop calculating collision for this enemy
        bool shouldIgnoreCollision() {
            if (iFrameCount - iFrameCollision < collisionIgnoreDur && iFrameCollision != -1) {
                return true;
            } else {
                return false;
            }
        }

        // Should be called during OnMeleeHit of enemy. If return true, their hit should be nullified by caller
        // Conditions: the frame is very close, or (the frame is relatively close and affectEnemyOnHit is 1)
        bool shouldNullifyEnemyCurretHit() {
            log::debug("shouldNullify? iFrameCollision: {}, iFrameCount:{}, affectEnemyOnHit:{}", iFrameCollision,
                       iFrameCount, affectEnemyOnHit);
            if (iFrameCollision == -1) return false;
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

        // For several frames, continue to spawn sparks on enemy's weapon
        void SpawnSpark() {
            if (bDisableSpark) return;
            if (iFrameCollision == -1 || !enemy) return;
            if (iFrameCount - iFrameCollision < iSparkSpawn && iFrameCount % 6 == 0) {
                if (GetSpecialRace(enemy) != 0) return;
                const auto nodeName = isEnemyLeft ? "SHIELD"sv : "WEAPON"sv;
                auto root = netimmerse_cast<RE::BSFadeNode*>(enemy->Get3D());
                if (!root) return;
                auto bone = netimmerse_cast<RE::NiNode*>(root->GetObjectByName(nodeName));
                if (!bone) return;
                auto enemyActor = enemy;
                auto posWeaponBottom = bone->world.translate;
                const auto weaponDirection = RE::NiPoint3{
                    bone->world.rotate.entry[0][1], bone->world.rotate.entry[1][1], bone->world.rotate.entry[2][1]};
                RE::NiPoint3 contactPosition = posWeaponBottom + weaponDirection * contactToEnemyBottom;
                // don't capture `this` since it may be destroyed before task executed
                SKSE::GetTaskInterface()->AddTask([enemyActor, contactPosition, bone]() { 
                    RE::NiPoint3 P_V = {0.0f, 0.0f, 0.0f};
                    RE::NiPoint3 contactPosition_tmp = contactPosition; // making compiler happy, since lambda is const by default
                    // Display spark on enemy's weapon, at the collision point
                    OnMeleeHit::play_impact_2(enemyActor, RE::TESForm::LookupByID<RE::BGSImpactData>(0x0004BB54), &P_V,
                                              &contactPosition_tmp, bone);
                });

            }
        }

        // For several frames, change the angle of enemy, creating hit juice
        void ChangeAngle() { 
            if (!enemy) return;
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
            if (!enemy) return;
            float x = hkv.quad.m128_f32[0];
            float y = hkv.quad.m128_f32[1];
            log::trace("Entering ChangeVelocity of enemy, hkv.x:{}, hkv.y:{}", x,
                       y);
            /*if (sqrt(x * x + y * y) < 10.0f) {
                return;
            }*/

            // If enemy is already far enough or this function is about to get no more call, set hkv to zero so they won't be pushed farther
            auto dist = enemy->GetPosition().GetDistance(
                enemyOriPos); 
            if (dist > fEnemyPushMaxDist || dist > pushEnemyMaxDist ||
                (iFrameCount - iFrameCollision == collisionIgnoreDur - 3)) { 
                hkv = hkv * 0.0f;
                if (enemy && enemy->GetCharController()) {
                    RE::hkVector4 tmp;
                    enemy->GetCharController()->GetLinearVelocityImpl(tmp);
                    tmp = tmp * 0.0f;  // reset speed
                    enemy->GetCharController()->SetLinearVelocityImpl(tmp);
                }
                return;
            }

            if (enemy && enemy->GetCharController()) {
                log::trace("About to change enemy speed, hkv length:{}", hkv.SqrLength3());
                RE::hkVector4 tmp;
                enemy->GetCharController()->GetLinearVelocityImpl(tmp);
                tmp = tmp + hkv; // new speed considers old speed
                //tmp = hkv;  // new speed doesn't consider old speed
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

        void Clear() {
            log::trace("In CollisionRing::Clear");
            for (size_t i = 0; i < capacity; i++) {
                buffer[i] = Collision();
            }
        }

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

    class PlayerCollision { // Only recording info related to player of the last collision
        int64_t frameLastCollision;
        float contactToBottom;
        bool isLeft;
        RE::hkVector4 pushVelocity;  // the velocity used to push player away
        int64_t frameToPush; // push player for how many frames
        float maxPushDis; // push player for at most how far. We check both this and fPlayerPushMaxDist
        RE::NiPoint3 posLastCollision;
        RE::Actor* playerAct;

    public:
        static PlayerCollision* GetSingleton() { 
            static PlayerCollision singleton;
            return std::addressof(singleton);
        }

        void Clear() { 
            frameLastCollision = 0;

        }

        bool IsEmpty() { return frameLastCollision == 0; }
        
        void SetValue(int64_t fLC, float cTB, bool iL, RE::hkVector4 hkv, int64_t fTP, float mPD,
                      RE::NiPoint3 pLC, RE::Actor* pA) {
            auto singleton = PlayerCollision::GetSingleton();
            singleton->frameLastCollision = fLC;
            singleton->contactToBottom = cTB;
            singleton->isLeft = iL;
            singleton->pushVelocity = hkv;
            singleton->frameToPush = fTP;
            singleton->maxPushDis = mPD;
            singleton->posLastCollision = pLC;
            singleton->playerAct = pA;
        }

        // For several frames, continue to spawn sparks on player's weapon
        void SpawnSpark() {
            if (bDisableSpark) return;
            if (!playerAct || frameLastCollision == 0) return;
            if (iFrameCount - frameLastCollision < iSparkSpawn && iFrameCount % 6 == 0) {
                const auto nodeName = isLeft ? "SHIELD"sv : "WEAPON"sv;
                auto root = netimmerse_cast<RE::BSFadeNode*>(playerAct->Get3D());
                if (!root) return;
                auto bone = netimmerse_cast<RE::NiNode*>(root->GetObjectByName(nodeName));
                if (!bone) return;
                auto playerActor = playerAct;
                auto posPlayerHand = bone->world.translate;
                const auto weaponDirection = RE::NiPoint3{
                    bone->world.rotate.entry[0][1], bone->world.rotate.entry[1][1], bone->world.rotate.entry[2][1]};
                RE::NiPoint3 contactPosition = posPlayerHand + weaponDirection * contactToBottom;
                // don't capture `this` since it may be destroyed before task executed
                SKSE::GetTaskInterface()->AddTask([playerActor, contactPosition, bone]() {
                    RE::NiPoint3 P_V = {0.0f, 0.0f, 0.0f};
                    RE::NiPoint3 contactPosition_tmp =
                        contactPosition;  // making compiler happy, since lambda is const by default
                    // Display spark on player's weapon, at the collision point
                    OnMeleeHit::play_impact_2(playerActor, RE::TESForm::LookupByID<RE::BGSImpactData>(0x0004BB54), &P_V,
                                              &contactPosition_tmp, bone);
                });
            }
        }

        // For several frames, change the velocity of player, to push them away
        void ChangeVelocity() {
            if (!playerAct || frameLastCollision == 0) return;
            float x = pushVelocity.quad.m128_f32[0];
            float y = pushVelocity.quad.m128_f32[1];
            log::trace("Entering ChangeVelocity of player, pushVelocity.x:{}, pushVelocity.y:{}", x, y);
            /*if (sqrt(x * x + y * y) < 10.0f) {
                return;
            }*/

            if (iFrameCount - frameLastCollision > frameToPush) return;

            // If player is already far enough or this function is about to get no more call, set hkv to zero so they
            // won't be pushed farther
            auto dist = playerAct->GetPosition().GetDistance(posLastCollision);
            if (dist > fPlayerPushMaxDist || dist > maxPushDis) {
                pushVelocity = pushVelocity * 0.0f;
                if (playerAct && playerAct->GetCharController()) {
                    RE::hkVector4 tmp;
                    playerAct->GetCharController()->GetLinearVelocityImpl(tmp);
                    tmp = tmp * 0.0f; // set player speed to 0
                    playerAct->GetCharController()->SetLinearVelocityImpl(tmp);
                }
                return;
            }

            if (playerAct && playerAct->GetCharController()) {
                log::trace("About to change player speed, pushVelocity length:{}", pushVelocity.SqrLength3());
                RE::hkVector4 tmp;
                playerAct->GetCharController()->GetLinearVelocityImpl(tmp);
                tmp = tmp + pushVelocity;  // new speed considers old speed
                //tmp = pushVelocity;
                playerAct->GetCharController()->SetLinearVelocityImpl(tmp);
            }

            return;
        }
    };

    class WeaponPos { // this class is just used to store player's weapon pos and calculate speed
    public:
        RE::NiPoint3 bottom;
        RE::NiPoint3 top;
        WeaponPos() = default;
        WeaponPos(RE::NiPoint3 b, RE::NiPoint3 t) : bottom(b), top(t) {}
    };

    class SpeedRing {
    public:
        std::vector<WeaponPos> bufferL;
        std::vector<WeaponPos> bufferR;
        std::size_t capacity; // how many latest frames are stored
        std::size_t indexCurrentL;
        std::size_t indexCurrentR;
        SpeedRing(std::size_t cap) : bufferL(cap), bufferR(cap), capacity(cap), indexCurrentR(0), indexCurrentL(0) {}
        
        void Clear() {

        }

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

        // The shortest distance between player's weapon and the given projectile, in the last N frames
        DistResult ShortestDisRecently(std::size_t N, RE::NiPoint3 posProj, RE::NiPoint3 velocity, bool isSlowed, bool ignoreLeft, float leftSpeed, float rightSpeed) {
            float shortestDist = 9999.0f;
            DistResult shortestResult = DistResult();
            float extraLength = 0.0f;
            if (!isSlowed) extraLength = 30.0f;
            RE::NiPoint3 posProjFakeHead = posProj + velocity / velocity.Length() * (fProjLength / 2 + extraLength); 
            posProj = posProj - velocity / velocity.Length() * (fProjLength / 2 + extraLength); 
            if (N == 0 || N > capacity) {
                // Return zero velocity or handle error
                return DistResult();
            }
            for (int i = 0; i < 2; i++) {
                bool isLeft = i == 0 ? true : false;
                if (isLeft && ignoreLeft) continue;
                auto speedWeap = isLeft ? leftSpeed : rightSpeed;
                if (speedWeap < fProjWeapSpeedThres) continue;
                std::size_t currentIdx = isLeft ? indexCurrentL : indexCurrentR;
                std::vector<WeaponPos>& buffer = isLeft ? bufferL : bufferR;
                for (std::size_t frameCount = 0; frameCount < N; frameCount++) {
                    std::size_t frame = (currentIdx - 1 - frameCount + capacity) % capacity;
                    RE::NiPoint3 posBottom = buffer[frame].bottom;
                    RE::NiPoint3 posTop = buffer[frame].top;
                    auto distResult = OnMeleeHit::Dist(posBottom, posTop, posProj, posProjFakeHead);
                    if (distResult.dist < shortestDist) {
                        shortestDist = distResult.dist;
                        distResult.proj_isLeft = isLeft;
                        shortestResult = distResult;
                    }
                }
            }
            return shortestResult;
        }
    };
    extern SpeedRing speedBuf; // positions of player's weapons, but the handle length is 0 and weapon length is a fixed number
    extern SpeedRing weapPosBuf;  // positions of player's weapons

    // Record information for timeslow spell
    class SlowTimeEffect {
    public: 
        int64_t frameShouldRemove;
        std::vector<float> oldMagnitude; 
        RE::SpellItem* timeSlowSpell;
        int64_t frameLastSlowTime; // don't allow too frequent slow time 
        SlowTimeEffect(std::size_t cap)
            : frameShouldRemove(-1), frameLastSlowTime(-1), oldMagnitude(cap), timeSlowSpell(nullptr) {}

        bool shouldRemove(int64_t currentFrame) {
            if (frameShouldRemove == -1) {
                return false;
            }
            return frameShouldRemove <= currentFrame;
        }

        void clear() { 
            frameShouldRemove = -1;
            oldMagnitude.clear();
        }
    };

    extern SlowTimeEffect slowTimeData;

    class ParriedProj {
    public:
        std::vector<RE::Projectile*> bufParried;
        std::vector<RE::Projectile*> bufSlowed;
        std::vector<int64_t> bufferFrame;
        std::vector<int64_t> bufferFrameSlow;
        std::vector<float> bufSlowVelZ;
        std::vector<bool> bufSlowRestored;
        std::size_t indexCurrent;
        std::size_t indexCurrentSlow;
        std::size_t capacity;

        //// Also has some data to log the previous positions of a projectile, to find arrows in wall
        //// Arrows in wall can't be filtered by linearVelocity, because it doesn't reduce to 0 after impact
        //class ProjRecord {
        //public:
        //    std::vector<RE::NiPoint3> bufferPos;
        //    std::size_t indexPosCurrent;
        //    std::size_t posCapacity;
        //    RE::Projectile* proj;
        //    bool isStatic;
        //    int64_t iFrameSetStatic; // if it's set to static too long ago, maybe the game is reusing pointer
        //    ProjRecord(std::size_t cap, RE::Projectile* proj)
        //        : bufferPos(cap), posCapacity(cap), indexPosCurrent(0), proj(proj), isStatic(false), iFrameSetStatic(-1) {}

        //};
        //std::vector<ProjRecord*> bufferProjRecord;
        //std::size_t indexRecordCurrent;

        ParriedProj(std::size_t cap)
            : bufParried(cap),
              bufSlowed(cap),
              bufferFrame(cap),
              bufferFrameSlow(cap),
              bufSlowVelZ(cap),
              bufSlowRestored(cap),
              indexCurrent(0),
              indexCurrentSlow(0),
              capacity(cap) {
            //for (size_t i = 0; i < cap; i++) {
            //    bufferProjRecord.push_back(nullptr);
            //}
        }

        void Clear() {
            log::trace("In ParriedProj::Clear");
            for (size_t i = 0; i < capacity; i++) {
                bufParried[i] = nullptr;
                bufSlowed[i] = nullptr;
                bufferFrame[i] = -1;
                bufSlowRestored[i] = false;
                bufSlowVelZ[i] = 0.0f;
                indexCurrent = 0;
                bufferFrameSlow[i] = -1;
                indexCurrentSlow = 0;
                //delete bufferProjRecord[i];
                //bufferProjRecord[i] = nullptr;
            }
        }

        //// Record any projectile, unless it's already known as static
        //void UpdateProjPos(RE::Projectile* proj) {
        //    for (size_t i = 0; i < capacity; i++) {
        //        if (bufferProjRecord[i] != nullptr && bufferProjRecord[i]->proj == proj) {
        //            bufferProjRecord[i]->bufferPos[bufferProjRecord[i]->indexPosCurrent] = proj->GetPosition();
        //            bufferProjRecord[i]->indexPosCurrent =
        //                (bufferProjRecord[i]->indexPosCurrent + 1) % bufferProjRecord[i]->posCapacity;
        //            log::info("UpdateProjPos: pos:({},{},{})", proj->GetPosition().x, proj->GetPosition().y,
        //                       proj->GetPosition().z);
        //            return;
        //        }
        //    }
        //    // This is a new proj
        //    bufferProjRecord[indexRecordCurrent] = new ProjRecord(10, proj);
        //    indexRecordCurrent = (indexRecordCurrent + 1) % capacity;
        //    log::info("UpdateProjPos: new proj");
        //}

        //bool IsStatic(RE::Projectile* proj) {
        //    for (size_t i = 0; i < capacity; i++) {
        //        if (bufferProjRecord[i] != nullptr && bufferProjRecord[i]->proj == proj) {
        //            /*if (iFrameCount - bufferProjRecord[i]->iFrameSetStatic > 90) {
        //                delete bufferProjRecord[i];
        //                bufferProjRecord[i] = nullptr;
        //                continue;
        //            }*/
        //            auto record = bufferProjRecord[i];
        //            //if (record->isStatic) return true;
        //            // check if it should be static
        //            RE::NiPoint3 startPos =
        //                record->bufferPos[(record->indexPosCurrent - 5 + record->posCapacity) % record->posCapacity];
        //            RE::NiPoint3 endPos =
        //                record->bufferPos[(record->indexPosCurrent - 1 + record->posCapacity) % record->posCapacity];
        //            if (startPos.SqrLength() < 0.1f || endPos.SqrLength() < 0.1f) {
        //                // too early to tell if it's static since we haven't record enough position info
        //                return false;
        //            }
        //            RE::NiPoint3 velocity = (endPos - startPos) / static_cast<float>(4);
        //            log::info("IsStatic: proj velocity:{}", velocity.SqrLength());
        //            if (velocity.SqrLength() < 300.0f) {
        //                record->isStatic = true;
        //                record->iFrameSetStatic = iFrameCount;
        //                return true;
        //            }
        //        }
        //    }
        //    return false;
        //}

        // Record a parried projectile
        void PushParried(RE::Projectile* proj) { 
            bufParried[indexCurrent] = proj;
            bufferFrame[indexCurrent] = iFrameCount;
            indexCurrent = (indexCurrent + 1) % capacity;
        }

        // When calculating isParried, we should also check the frame: projectile pointer seems to be reused by the game
        bool IsParried(RE::Projectile* proj) {
            for (size_t i = 0; i < capacity; i++) {
                if (bufParried[i] == proj) {
                    if (iFrameCount - bufferFrame[i] < 60) {
                        return true;
                    }
                }
            }
            return false;
        }

        void PushSlowed(RE::Projectile* proj, float velZ) {
            bufSlowed[indexCurrentSlow] = proj;
            bufferFrameSlow[indexCurrentSlow] = iFrameCount;
            bufSlowRestored[indexCurrentSlow] = false;
            bufSlowVelZ[indexCurrentSlow] = velZ;
            indexCurrentSlow = (indexCurrentSlow + 1) % capacity;
        }

        size_t IsSlowed(RE::Projectile* proj) {
            for (size_t i = 0; i < capacity; i++) {
                if (bufSlowed[i] == proj) {
                    if (iFrameCount - bufferFrameSlow[i] < 90) {
                        return i;
                    }
                }
            }
            return 99999;
        }

    };
    extern ParriedProj parriedProj;

    void FillShieldCenterNormal(RE::Actor* actor, RE::NiPoint3& shieldCenter, RE::NiPoint3& shieldNormal);
}

