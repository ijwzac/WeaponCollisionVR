#include "Settings.h"

using namespace SKSE;
using namespace SKSE::log;

// Global
bool bEnableWholeMod = true;
int globalInputCounter = 0;
int64_t iFrameLastCollision = 0;
bool bHandToHandLoad = false;
float fRangeMulti = 1.0f;
float fCollisionDistThres = 15.0f;
float fDetectEnemy = 600.f;
bool bShowWeaponSegment = false;
int64_t iFrameCount = 0;
bool bEnableTrace = false;
bool bPlayerMustBeAttacking = false;

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
int64_t collisionEffectDurPlayerShort = 20;
float fPlayerStaCostMin = 10.0f;
float fPlayerStaCostMax = 60.0f;
float fPlayerStaCostWeapMulti = 1.0f;
float fPlayerWeaponSpeedRewardThres = 150.0f;
float fPlayerWeaponSpeedReward = 0.25f;
float fPlayerWeaponSpeedRewardThres2 = 30.0f;
float fPlayerWeaponSpeedReward2 = 0.5f;
float fPlayerStaStopThresPer = 0.0f;
float fPlayerStaLargeRecoilThresPer = 0.0f;
int iHapticStrMin = 10;
int iHapticStrMax = 50;
float fHapticMulti = 1.0f;
int iHapticLengthMicroSec = 100000;  // 100 ms
float fExpBlock = 3.0f; // At level 20, block needs [86] exp to reach 21. The number 86 increases non-linearly as the skill levels up
float fExpOneHand = 2.0f; // At level 20, one-handed needs [110] exp to reach 21.
float fExpTwoHand = 2.0f; // [179]
float fExpHandToHand = 2.0f; // [7] in vanilla, but this number may have been changed by Hand To Hand

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
    constexpr auto path = L"Data/SKSE/Plugins/WeaponCollision.ini";

    CSimpleIniA ini;
    ini.SetUnicode();

    auto err = ini.LoadFile(path);

    sMain.Load(ini);
    sDifficulty.Load(ini);
    sEffectOnEnemy.Load(ini);
    sEffectOnPlayer.Load(ini);
    sExperience.Load(ini);
    sHaptic.Load(ini);
    sTechnique.Load(ini);

    err = ini.SaveFile(path);
}

void Settings::Main::Load(CSimpleIniA& a_ini) { 
    static const char* section = "Main";

    detail::get_value(
        a_ini, bEnableWholeMod, section, "EnableWholeMod",
        ";;;;;;;;;;;;;;;;;;;;;;;;\n; While playing the game, you can DYNAMICALLY CHANGE SETTINGS below. Steps:\n"
        ";(1) Edit settings; (2) Save and close this file; (3) When not in combat, open Skyrim console by \"`\"\n"
        ";(4) Close the console, no need to type anything; (5) Now settings are updated.\n;;;;;;;;;;;;;;;;;;;;;;;;\n;\n;\n"
        "; Set this to false if you want to completely disable this mod. Default:\"true\".");
}

void Settings::Difficulty::Load(CSimpleIniA& a_ini) {
    static const char* section = "Important Settings";

    detail::get_value(a_ini, bPlayerMustBeAttacking, section, "PlayerMustBeAttacking",
        ";=======Parry difficulty=======\n"
        ";\n"
        "; When this is \"true\", player must be attacking to trigger a parry.\n"
        "; If you are a SE or AE player, I am setting this to \"true\" while set CollisionDistance to a higher value.\n"
        ";         Otherwise the parry is annoyingly too easy or too hard.\n"
        "; If you are a VR player, I am setting this to \"false\" while set CollisionDistance to a lower value.\n"
        ";         Otherwise you can't parry naturally. \n"
        "; Default:\"true\" for SE or AE player, \"false\" for VR player");

    detail::get_value(
        a_ini, fCollisionDistThres, section, "CollisionDistance",
        "; To trigger a collision between two weapons, how close weapons should be.\n"
        "; Higher value means easier parry. Recommend less than 30.0 for VR players, and 30.0~50.0 for SE and AE players. Unit: around 1.4 centimeter. Default:\"15.0\" for VR, \"50.0\" for SE/AE");

    detail::get_value(a_ini, fRangeMulti, section, "WeaponRangeMultiply",
        "; The length of weapon is multiplied by this value. This length is only used by this mod.\n"
        "; Higher value means easier parry. \"1.0\" matches the models of most weapons pretty good. Default:\"1.0\" for VR, \"1.35\" for SE/AE");

    detail::get_value(a_ini, fEnemyStaStopThresPer, section, "EnemyStopThreshold",
        ";=======Enemy stagger=======\n"
        ";\n"
        "; When enemy's stamina is below this ratio, they will stop their attack combo. Set this to \"1.0\" to always stop them\n"
        "; Set this to \"0.3\" to stop them when their stamina is below 30%. Default:\"0.3\"");

    detail::get_value(a_ini, fEnemyStaLargeRecoilThresPer, section, "EnemyStaggerThreshold",
        "; When enemy's stamina is below this ratio, they will stop their attack combo and stagger. Set this to "
        "\"1.0\" to always stop and stagger them\n"
        "; Set this to \"0.05\" to stop them when their stamina is below 5%. Default:\"0.05\"");

    detail::get_value(a_ini, fPlayerStaStopThresPer, section, "PlayerStopThreshold",
        ";=======Player stagger=======\n"
        ";\n"
        "; Similar to above, when player's stamina is below this ratio, they will stop their attack combo.\n"
        "; Setting this to \"0.0\" makes player always able to continue attack. Recommend VR players to set this to \"0.0\"\n"
        "; Default:, \"0.3\" for SE or AE player, \"0.0\" for VR player");

    detail::get_value(
        a_ini, fPlayerStaLargeRecoilThresPer, section, "PlayerStaggerThreshold",
        "; Similar to above, when player's stamina is below this ratio, they will stop their attack combo and stagger.\n"
        "; Setting this to \"0.0\" makes player always not staggered. Recommend VR players to set this to \"0.0\"\n"
        "; Default:, \"0.05\" for SE or AE player, \"0.0\" for VR player");
}

void Settings::EffectOnEnemy::Load(CSimpleIniA& a_ini) {
    static const char* section = "Parry Effect on Enemies";

    detail::get_value(
        a_ini, fEnemyPushMulti, section, "EnemyPushMagnitude",
        ";=======Hit effect=======\n"
        ";\n"
        "; After a parry, how fast the enemy should be pushed away. This creates a little juice as parry feedback\n"
        "; Higher value means pushing faster and probably farther. Unit:~ 1.4 cm per second. Default:\"750.0\"");

    detail::get_value(a_ini, fEnemyPushMaxDist, section, "EnemyPushDistance",
        "; After a parry, how far the enemy should be pushed away.\n"
        "; Higher value means pushing farther. Unit:~ 1.4 cm. Default:\"30.0\"");

    detail::get_value(
        a_ini, fEnemyRotStep, section, "EnemyRotationMultiplier",
        "; After a parry, how fast the enemy should rotate. This creates a little juice as parry feedback. The rotation direction and duration is determined by the "
        "speed of your weapon.\n"
        "; Higher value means faster and more obvious rotation. Unit:~ 51 degrees per second. Default:\"0.15\"");

    
    detail::get_value(
        a_ini, fEnemyStaCostWeapMulti, section, "EnemyStaminaCostMultiplier",
        ";=======Stamina cost=======\n"
        ";\n"
        "; Stamina_cost_to_enemy = EnemyStaminaCostMultiplier * (player_weapon_damage * "
        "player_related_skill * 1.5 - enemy_weapon_damage * enemy_related_skill)\n"
        "; Stamina_cost_to_enemy is doubled if player is power attacking and enemy is not\n"
        "; The final Stamina_cost_to_enemy will be modified to fit in [EnemyMinStaminaCost, EnemyMaxStaminaCost]\n"
        ";\n"
        "; As shown in the formula above, the stamina cost to enemy is multiplied by this value. Default:\"1.0\"");


    detail::get_value(a_ini, fEnemyStaCostMin, section, "EnemyMinStaminaCost",
        "; The minimal stamina cost to the enemy for each parry. Default:\"10.0\"");

    detail::get_value(a_ini, fEnemyStaCostMax, section, "EnemyMaxStaminaCost",
        "; The maximal stamina cost to the enemy for each parry. Default:\"60.0\"");

}

void Settings::EffectOnPlayer::Load(CSimpleIniA& a_ini) {
    static const char* section = "Parry Effect on Player";

    //detail::get_value(a_ini, collisionEffectDurPlayerShort, section, "PlayerHitNullifyDuration",
    //    "; If player hits an enemy within this amount of frames after a parry, it's nullified. Default:\"20.0\"");

    detail::get_value(
        a_ini, fPlayerStaCostWeapMulti, section, "PlayerStaminaCostMultiplier",
        ";=======Stamina cost=======\n"
        ";\n"
        "; Stamina_cost_to_player = PlayerStaminaCostMultiplier * (enemy_weapon_damage * "
        "enemy_related_skill * 1.5 - player_weapon_damage * player_related_skill)\n"
        "; Stamina_cost_to_player is doubled if enemy is power attacking and player is not\n"
        "; The final Stamina_cost_to_player will be modified to fit in [PlayerMinStaminaCost, PlayerMaxStaminaCost]\n"
        ";\n"
        "; After all modification, VR player can reduce their actual stamina cost by a mechanism described in \"Weapon speed reward\" below\n"
        "; SE and AE players already have their stamina cost reduced, compared to VR players\n"
        ";\n"
        "; As shown in the formula above, the stamina cost to player is multiplied by this value. Default:\"1.0\"");

    detail::get_value(a_ini, fPlayerStaCostMin, section, "PlayerMinStaminaCost",
                      "; The minimal stamina cost to the player for each parry. Default:\"10.0\"");

    detail::get_value(a_ini, fPlayerStaCostMax, section, "PlayerMaxStaminaCost",
                      "; The maximal stamina cost to the player for each parry. Default:\"60.0\"");

    detail::get_value(
        a_ini, fPlayerWeaponSpeedRewardThres, section, "PlayerWeaponSpeedRewardThreshold1",
        ";=======Weapon speed reward VR=======\n"
        ";\n"
        "; (SE and AE players don't have controller, so stamina cost is already reduced for them, and settings below is turned off)\n"
        "; When player is moving their weapon at some speed, they will be rewarded by reducing their stamina cost\n"
        "; Stamina_cost_to_player is multiplied by PlayerWeaponSpeedReward1, if their weapon speed is higher than PlayerWeaponSpeedRewardThreshold1\n"
        "; If not passing Threshold1, Stamina_cost_to_player is multiplied by PlayerWeaponSpeedReward2, if their weapon speed is higher than PlayerWeaponSpeedRewardThreshold2\n"
        "; When player is swinging their weapon pretty fast, they reduce their stamina cost by a lot.\n"
        "; \"150.0\" is about swinging your controller to attack. Unit:~ 1.5 cm per second. Default:\"150.0\"");

    detail::get_value(a_ini, fPlayerWeaponSpeedReward, section, "PlayerWeaponSpeedReward1",
        "; The Stamina_cost_to_player is multiplied by this value, if passing PlayerWeaponSpeedRewardThreshold1.\n"
        "; \"0.25\" means the stamina cost is reduced by 75%. Default:\"0.25\"");

    detail::get_value(
        a_ini, fPlayerWeaponSpeedRewardThres2, section, "PlayerWeaponSpeedRewardThreshold2",
        "; This takes effect when PlayerWeaponSpeedRewardThreshold1 is not passed."
        "; When player is at least moving their weapon instead of holding it still, they reduce their stamina cost.\n"
        "; \"30.0\" is very easy to pass. Unit:~ 1.5 cm per second. Default:\"30.0\"");

    detail::get_value(
        a_ini, fPlayerWeaponSpeedReward2, section, "PlayerWeaponSpeedReward2",
        "; The Stamina_cost_to_player is multiplied by this value, if passing PlayerWeaponSpeedRewardThreshold2.\n"
        "; \"0.50\" means the stamina cost is reduced by 50%. Default:\"0.50\"");
}

void Settings::Experience::Load(CSimpleIniA& a_ini) {
    static const char* section = "Player Experience Gain";

    detail::get_value(a_ini, fExpBlock, section, "BlockExperienceGain",
                      "; Each collision gives player this amount of block experience. Default:\"3.0\"");
    detail::get_value(a_ini, fExpOneHand, section, "OneHandedExperienceGain",
                      "; Each collision gives player this amount of one-handed experience, if parrying with one-handed weapon. Default:\"2.0\"");
    detail::get_value(a_ini, fExpTwoHand, section, "TwoHandedExperienceGain",
                      "; Each collision gives player this amount of two-handed experience, if parrying with two-handed weapon. Default:\"2.0\"");
    detail::get_value(a_ini, fExpHandToHand, section, "HandToHandExperienceGain",
                      "; If Hand to Hand - An Adamant Addon and Adamant are installed, and player is parrying with fists\n"
        "; (VR players should install Hand to Hand - An Adamant VR Addon and the base mod) \n"
        "; Each collision gives player this amount of hand-to-hand experience. Default:\"2.0\"");
}

void Settings::Haptic::Load(CSimpleIniA& a_ini) {
    static const char* section = "VR Controller Haptic Vibration";

    detail::get_value(a_ini, fHapticMulti, section, "HapticStrengthMultiplier",
        "; haptic_strength_on_VR_controller = Stamina_cost_to_player * HapticStrengthMultiplier.\n"
        "; The final haptic_strength_on_VR_controller is modified to fit in [HapticStrengthMin, HapticStrengthMax].\n"
        ";\n"
        "; As shown above, the actual stamina cost multiplies this value is the haptic strength. Default:\"1.0\"");

    detail::get_value(a_ini, iHapticStrMin, section, "HapticStrengthMin",
         "; The minimal haptic strength. Default:\"10\"");

    detail::get_value(a_ini, iHapticStrMax, section, "HapticStrengthMax",
        "; The maximal haptic strength. Default:\"50\"");

    detail::get_value(a_ini, iHapticLengthMicroSec, section, "HapticDuration",
        "; The haptic duration. After configured here, it won't be changed by game events. Unit: microsecond. \"100000\" is 0.1 seconds. Default:\"100000\"");
}

void Settings::Technique::Load(CSimpleIniA& a_ini) {
    static const char* section = "Other Settings for Debug and Internal Usage";

    detail::get_value(
        a_ini, bEnableTrace, section, "EnableTrace",
        "; WARNING: normally you don't need to change any value below. Please read instructions carefully before any "
        "change.\n"
        ";\n"
        "; Turn on the trace, printing to C:\\Users\\XXX\\Documents\\My Games\\Skyrim VR\\SKSE\\WeaponCollision.log\n"
        "; In worst case, turning this on makes the file grow at 10MB/minute, and might slow the game down, but useful when you report bug to me. Default:\"false\"");


    detail::get_value(a_ini, fDetectEnemy, section, "EnemyDetectionRange",
        "; We only calculate weapon collisions between player and enemies in this range, for optimization.\n"
        "; Unit:~ 1.5 cm. Default:\"600.0\"");

    detail::get_value(
        a_ini, bShowWeaponSegment, section, "DebugShowWeaponRange",
        "; WARNING: May hurt your eyes. Display sparks on the two ends of player's and enemies' weapons.\n"
        "; I turn this on to verify if the weapon length used by this mod is correct. Default:\"false\"");

    detail::get_value(
        a_ini, collisionIgnoreDur, section, "collisionIgnoreDur",
        "; After a collision, for how many frames should we ignore following collisions of the same enemy.\n"
        "; A value too low creates lots of collision for each parry. Default:\"30\"");

    detail::get_value(
        a_ini, collisionEffectDurEnemyShort, section, "NullifyEnemyAllAttackFrames",
        "; After a collision, within how many frames should we nullify all melee hits from this enemy.\n"
        "; Default:\"30\"");

    detail::get_value(
        a_ini, collisionEffectDurEnemyLong, section, "NullifyEnemyFirstAttackFrames",
        "; After a collision, within how many frames should we nullify the first melee hit from this enemy.\n"
        "; Default:\"90\"");

    detail::get_value(
        a_ini, iDelayEnemyHit, section, "DelayEnemyHitOnPlayer",
        "; This one is tricky but important. This mod delays melee hit events from enemy to player,\n"
        "; because in Skyrim enemies have some cheating attack animations, where they hit you a few frames before their weapon reaches you\n"
        "; These attack animations are unfairly hard to parry. A delay for 6 frames is nearly unnoticeable and fixes this problem.\n"
        "; Decrease it if your framerate is below 60fps. Let me know if this feature causes any trouble to other mods.\n"
        "; Default:\"6\"");
}
    