#include "Settings.h"

// Global
bool bEnableWholeMod = true;
int globalInputCounter = 0;
bool bHandToHandLoad = false;
float fRangeMulti = 1.0f;
float fCollisionDistThres = 10.0f;
float fDetectEnemy = 600.f;
bool bShowWeaponSegment = true;
int64_t iFrameCount = 0;

// Enemy
float fEnemyPushMulti = 750.0f;
float fEnemyPushMaxDist = 30.0f;
float fEnemyRotStep = 0.15f;
float fEnemyStaCostMin = 10.0f;
float fEnemyStaCostMax = 60.0f;
float fEnemyStaCostWeapMulti = 1.0f;
float fEnemyStaStopThresPer = 0.3f;
float fEnemyStaLargeRecoilThresPer = 0.05f;

// Player
float fPlayerPushMulti = 0.0f;
float fPlayerStaCostMin = 6.0f;
float fPlayerStaCostMax = 50.0f;
float fPlayerStaCostWeapMulti = 1.0f;
float fPlayerWeaponSpeedRewardThres = 150.0f;
float fPlayerWeaponSpeedReward = 0.25f;
float fPlayerWeaponSpeedRewardThres2 = 30.0f;
float fPlayerWeaponSpeedReward2 = 0.5f;
int iHapticStrMin = 10;
int iHapticStrMax = 50;
float fHapticMulti = 1.0f;
int iHapticLengthMicroSec = 100000;  // 100 ms
float fExpBlock = 3.0f; // At level 20, block needs [86] exp to reach 21. The number 86 increases non-linearly as the skill levels up
float fExpOneHand = 2.0f; // At level 20, one-handed needs [110] exp to reach 21.
float fExpTwoHand = 3.0f; // [179]
float fExpHandToHand = 1.0f; // [7]

int64_t collisionIgnoreDur = 30;
int64_t collisionEffectDurEnemyShort = 30;  // Within 30 frames, any attack from the enemy is nullified
int64_t collisionEffectDurEnemyLong =
    90;  // Within 90 frames, only the first attack from enemy will be nullified
         // Is there any attack animation whose start and hit will be longer than 90 frames?
int64_t iDelayEnemyHit = 6;


Settings* Settings::GetSingleton() {
    static Settings singleton;
    return std::addressof(singleton);
}

void Settings::Load() {
    constexpr auto path = L"Data/SKSE/Plugins/ParryingRPG.ini";

    CSimpleIniA ini;
    ini.SetUnicode();

    ini.LoadFile(path);

    sMain.Load(ini);
    scores.Load(ini);

    ini.SaveFile(path);
}

void Settings::Main::Load(CSimpleIniA& a_ini) { 
    static const char* section = "Main";

    detail::get_value(
        a_ini, bEnableWholeMod, section, "EnableWholeMod",
        "=====While playing game, you can also change any setting below. Steps:\n"
        "(1) Edit settings; (2) Save and close file; (3) Open Skyrim console by \"`\";\n"
        "(4) Close the console, no need to type anything; (5) Now settings are updated.\n=====\n\n"
        "; Set this to false if you want to completely disable this mod. Default:\"true\".");
}

void Settings::Scores::Load(CSimpleIniA& a_ini) {
    static const char* section = "Scores";


    detail::get_value(
        a_ini, collisionIgnoreDur, section, "collisionIgnoreDur",
        "; After a collision, for how many frames should we ignore following collisions of the same enemy. Default:\"30\"");
}