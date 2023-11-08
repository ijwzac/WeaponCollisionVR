#pragma once

// Whole mod
extern bool bEnableWholeMod;

// Global
extern int64_t iFrameCount;
extern bool bHandToHandLoad;
extern bool bPlanck;

// Parry difficulty
extern float fRangeMulti;          // controls the effective length of weapon in this mod
extern float fCollisionDistThres;  // if two weapons are closer than this number, it's a collision
extern bool bPlayerMustBeAttacking; // Recommended for non-VR players to turn on. VR player should turn off.

// Projectile Parry
extern bool bEnableProjParry;
extern float fProjDetectRange; // The range of projectile detection
extern float fProjLength; // The segment of projectile is from its position to a point ahead of it, calculated using its velocity
extern float fProjCollisionDistThres; // If weapon and projectile distance is smaller than this number, it's a collision
extern int64_t iProjCollisionFrame; // Collision is calculated for player's weapon positions for the last X frames
extern float fAutoAimThres;         // If the cos() of player weapon velocity and enemy position 
                                    // is greater than this, aim parried projectile to enemy
extern int64_t iTimeSlowFrameProj;
extern int64_t iTimeSlowFrameProjAutoAim;

// Parry effect on enemy
extern float fEnemyPushVelocityMulti;  // the speed multiplier that the enemy will be pushed
extern float fEnemyPushMaxDist;  // the max distance the enemy will be pushed
extern float fEnemyRotStep;      // the angle enemy rotates every frame. Frame decided by RotateFrame()
extern float fEnemyStaCostMin;   // minimal stamina cost to enemy
extern float fEnemyStaCostMax;   // max stamina cost to enemy. Power attack X2 is after this check
extern float fEnemyStaCostWeapMulti;        // multiplier of stamina cost.
                                            // The value after mutliply still need to fit in the min and max
extern float fEnemyStaStopThresPer;         // when enemy stamina below this percent, they stop current attack
extern float fEnemyStaLargeRecoilThresPer;  // when enemy stamina below this percent, they stop and have large recoil

extern float fEnemyStopVelocityThres;       // When player's weapon speed is above this, enemy stop current attack
extern float fEnemyLargeRecoilVelocityThres;  // When player's weapon speed is above this, enemy stop current attack and recoil
extern float fEnemyHealthRagdollThresPer;      // when enemy health below this percent, ragdoll them

// Parry effect on player
// Note: even if player's stamina is 0, they can still parry
extern float fPlayerPushVelocityMulti;         // the speed multiplier that the player will be pushed
extern float fPlayerPushMaxDist;                // the max distance the player will be pushed
extern int64_t collisionEffectDurPlayerShort;  // When player hits enemy within this amount of frames, nullify this hit.
extern float fPlayerStaCostMax;              // max stamina cost to player
extern float fPlayerStaCostWeapMulti;        // multiplier of enemy's attack power.
extern float fPlayerStaCostMin;              // multiplier of stamina cost.
                                             // The value after mutliply still need to fit in the min and max
extern float fPlayerWeaponSpeedRewardThres;  // when player moves weapon at higher speed than this, their stamina cost
                                             // is reduced by half
extern float fPlayerWeaponSpeedReward;       // Multiplier os stamina cost if high speed
extern float fPlayerWeaponSpeedRewardThres2;
extern float fPlayerWeaponSpeedReward2;
extern float fPlayerStaUnableParryThresPer;         // when player stamina below this percent, they can't parry
extern float fPlayerStaStopThresPer;         // when player stamina below this percent, they stop current attack
extern float fPlayerStaLargeRecoilThresPer;  // when player stamina below this percent, they stop and have large recoil
extern int64_t iSparkSpawn; // number of frames that we spawn spark on weapons.
extern float fTimeSlowRatio; // 0.1 means time flows at 10% of normal time
extern int64_t iTimeSlowFrameNormal; // for how many frames will time be slow
extern int64_t iTimeSlowFrameStop;
extern int64_t iTimeSlowFrameLargeRecoil;
extern bool bPlayerCheckHeavyGauntlet;

// Player experience obtained for every collision
extern float fExpBlock;
extern float fExpOneHand;
extern float fExpTwoHand;
extern float fExpHandToHand; // Only useful when Adaman - Hand to Hand VR patch is installed

// Haptic feedback
extern int iHapticStrMin;
extern int iHapticStrMax;
extern float fHapticMulti;  // same logic above. Haptic decided by stamina cost
extern int iHapticLengthMicroSec; 

// Settings for internal usage. Don't change unless you understand the code
extern bool bEnableTrace; // Turn on the trace
extern float fDetectEnemy; // Calculate collisions between player and only enemies within this range
extern int64_t collisionIgnoreDur; // After a collision, within this number of frames, don't compute collision of the same enemy
                                    // This is to prevent having tens of collisions when weapons are close
                                    // 
extern bool bShowPlayerWeaponSegment; // For debug. This shows the weapon range in this mod. May hurt your eyes
extern bool bShowEnemyWeaponSegment; // For debug. This shows the weapon range in this mod. May hurt your eyes
extern int64_t collisionEffectDurEnemyShort; // after a collision, within this number of frames, a hit event from enemy must be affected
extern int64_t
    collisionEffectDurEnemyLong;  // after a collision, within this number of frames, 
                            // a hit event is affected if affectEnemyOnHit/affectPlayerOnHit is 1
extern int64_t iDelayEnemyHit; // When enemy hit player, delay that event for how many frames
extern float fMagicNum1;
extern float fMagicNum2;
extern float fMagicNum3;


// Thanks to: https://github.com/powerof3/CLibUtil
namespace string {
    template <class T>
    T to_num(const std::string& a_str, bool a_hex = false) {
        const int base = a_hex ? 16 : 10;

        if constexpr (std::is_same_v<T, double>) {
            return static_cast<T>(std::stod(a_str, nullptr));
        } else if constexpr (std::is_same_v<T, float>) {
            return static_cast<T>(std::stof(a_str, nullptr));
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
            return static_cast<T>(std::stol(a_str, nullptr, base));
        } else if constexpr (std::is_same_v<T, std::uint64_t>) {
            return static_cast<T>(std::stoull(a_str, nullptr, base));
        } else if constexpr (std::is_signed_v<T>) {
            return static_cast<T>(std::stoi(a_str, nullptr, base));
        } else {
            return static_cast<T>(std::stoul(a_str, nullptr, base));
        }
    }

    inline std::vector<std::string> split(const std::string& a_str, std::string_view a_delimiter) {
        auto range = a_str | std::ranges::views::split(a_delimiter) |
                     std::ranges::views::transform([](auto&& r) { return std::string_view(r); });
        return {range.begin(), range.end()};
    }

    // https://stackoverflow.com/a/35452044
    inline std::string join(const std::vector<std::string>& a_vec, std::string_view a_delimiter) {
        return std::accumulate(a_vec.begin(), a_vec.end(), std::string{},
                               [a_delimiter](const auto& str1, const auto& str2) {
                                   return str1.empty() ? str2 : str1 + a_delimiter.data() + str2;
                               });
    }
};  // namespace string

class Settings {
public:
    [[nodiscard]] static Settings* GetSingleton();

    void Load();

    struct Main {
        void Load(CSimpleIniA& a_ini);
    } sMain;

    struct Difficulty {
        void Load(CSimpleIniA& a_ini);
    } sDifficulty;

    struct EffectOnEnemy {
        void Load(CSimpleIniA& a_ini);
    } sEffectOnEnemy;

    struct EffectOnPlayer {
        void Load(CSimpleIniA& a_ini);
    } sEffectOnPlayer;

    struct Experience {
        void Load(CSimpleIniA& a_ini);
    } sExperience;

    struct Feedback {
        void Load(CSimpleIniA& a_ini);
    } sFeedback;

    struct Technique {
        void Load(CSimpleIniA& a_ini);
    } sTechnique;

private:
    Settings() = default;
    Settings(const Settings&) = delete;
    Settings(Settings&&) = delete;
    ~Settings() = default;

    Settings& operator=(const Settings&) = delete;
    Settings& operator=(Settings&&) = delete;

    struct detail {

        // Thanks to: https://github.com/powerof3/CLibUtil
        template <class T>
        static T& get_value(CSimpleIniA& a_ini, T& a_value, const char* a_section, const char* a_key, const char* a_comment,
                            const char* a_delimiter = R"(|)") {
            if constexpr (std::is_same_v<T, bool>) {
                a_value = a_ini.GetBoolValue(a_section, a_key, a_value);
                a_ini.SetBoolValue(a_section, a_key, a_value, a_comment);
            } else if constexpr (std::is_floating_point_v<T>) {
                a_value = static_cast<float>(a_ini.GetDoubleValue(a_section, a_key, a_value));
                a_ini.SetDoubleValue(a_section, a_key, a_value, a_comment);
            } else if constexpr (std::is_enum_v<T>) {
                a_value = string::template to_num<T>(
                    a_ini.GetValue(a_section, a_key, std::to_string(std::to_underlying(a_value)).c_str()));
                a_ini.SetValue(a_section, a_key, std::to_string(std::to_underlying(a_value)).c_str(), a_comment);
            } else if constexpr (std::is_arithmetic_v<T>) {
                a_value = string::template to_num<T>(a_ini.GetValue(a_section, a_key, std::to_string(a_value).c_str()));
                a_ini.SetValue(a_section, a_key, std::to_string(a_value).c_str(), a_comment);
            } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                a_value = string::split(a_ini.GetValue(a_section, a_key, string::join(a_value, a_delimiter).c_str()),
                                        a_delimiter);
                a_ini.SetValue(a_section, a_key, string::join(a_value, a_delimiter).c_str(), a_comment);
            } else {
                a_value = a_ini.GetValue(a_section, a_key, a_value.c_str());
                a_ini.SetValue(a_section, a_key, a_value.c_str(), a_comment);
            }
            return a_value;
        }
    };
};

// Code from: https://www.youtube.com/watch?v=afGRuSM2IIc
class EventProcessor : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    // Pretty typical singleton setup
    // *Private* constructor/destructor
    // And we *delete* the copy constructors and move constructors.
    EventProcessor() = default;
    ~EventProcessor() = default;
    EventProcessor(const EventProcessor&) = delete;
    EventProcessor(EventProcessor&&) = delete;
    EventProcessor& operator=(const EventProcessor&) = delete;
    EventProcessor& operator=(EventProcessor&&) = delete;

public:
    // Returns a reference to the one and only instance of EventProcessor :)
    //
    // Note: this is returned as a & reference. When you need this as a pointer, you'll want to use & (see below)
    static EventProcessor& GetSingleton() {
        static EventProcessor singleton;
        return singleton;
    }

    // Log information about Menu open/close events that happen in the game
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        // logger::trace("Menu {} Open? {}", event->menuName, event->opening); 
        if (event->menuName == "Console"sv && event->opening == false) {
            logger::trace("Console close. Now reload config"); 
            Settings::GetSingleton()->Load();
            spdlog::level::level_enum level = spdlog::level::info;
            if (bEnableTrace) level = spdlog::level::trace;
            spdlog::default_logger()->set_level(level);
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};