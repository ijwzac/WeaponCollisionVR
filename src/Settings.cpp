#include "Settings.h"

using namespace SKSE;
using namespace SKSE::log;

// Global
bool bEnableWholeMod = true;
bool bHandToHandLoad = false;
bool bPlanck = false;
float fRangeMulti = 1.0f;   
float fCollisionDistThres = 15.0f;
float fDetectEnemy = 600.f;
bool bShowPlayerWeaponSegment = false;
bool bShowEnemyWeaponSegment = false;
int64_t iFrameCount = 0;
int64_t iFrameTriggerPress = 0;
int64_t iFrameSlowCost = 0;
int64_t iFrameStopBlock = 0;
int iTraceLevel = 2;
bool bPlayerMustBeAttacking = false;
int64_t iSparkSpawn = 6;
std::chrono::steady_clock::time_point last_time;


// Projectile Parry
bool bEnableProjParry = true;
float fProjDetectRange = 700.0f;  // The range of projectile detection
float fProjCollisionDistThres = 12.0f;         // If weapon and projectile distance is smaller than this number, it's a collision
float fProjLength = 50.0f;
int64_t iProjCollisionFrame = 5;  // Collision is calculated for player's weapon positions for the last X frames
int64_t iTimeSlowFrameProj = 30;
float fProjSlowRatio = 0.20f;
float fProjSlowRadius = 400.0f;
int64_t iProjSlowFrame = 60;
float fProjSlowCost = 10.0f;
uint32_t iProjSlowButton1 = 33;
uint32_t iProjSlowButton2 = 33;

// Shield collision
bool bEnableShieldCollision = true;
bool bTreatShieldAsParry = false;
float fShieldRadius = 40.0f;
int64_t iFrameBlockDur = 20;
float fOriginEnterAngle = 30.0f;
float fOriginExitAngle = 35.0f;

// Enemy
float fEnemyPushVelocityMulti = 8.0f;
float fEnemyPushMaxDist = 35.0f;
float fEnemyRotStep = 0.15f;
float fEnemyStaCostMin = 12.0f;
float fEnemyStaCostMax = 60.0f;
float fEnemyStaCostWeapMulti = 1.0f;
float fEnemyStaStopThresPer = 0.0f;
float fEnemyStaLargeRecoilThresPer = 0.00f;
float fEnemyStopVelocityThres = 40.0f;
float fEnemyLargeRecoilVelocityThres = 130.0f;
float fEnemyHealthRagdollThresPer = 0.25f;

// Player
float fPlayerPushVelocityMulti = 8.0f;
float fPlayerPushMaxDist = 60.0f;
int64_t collisionEffectDurPlayerShort = 20;
float fPlayerStaCostMin = 12.0f;
float fPlayerStaCostMax = 40.0f;
float fPlayerStaCostWeapMulti = 1.0f;
float fPlayerWeaponSpeedRewardThres = 150.0f;
float fPlayerWeaponSpeedReward = 0.35f;
float fPlayerWeaponSpeedRewardThres2 = 30.0f;
float fPlayerWeaponSpeedReward2 = 0.7f;
float fPlayerStaUnableParryThresPer = 0.1f;
float fPlayerStaStopThresPer = 0.0f;
float fPlayerStaLargeRecoilThresPer = 0.0f;
bool bPlayerCheckHeavyGauntlet = false;

// Feedback
int64_t iTimeSlowFrameNormal = 12;
int64_t iTimeSlowFrameStop = 25;
int64_t iTimeSlowFrameLargeRecoil = 50;
float fTimeSlowRatio = 0.1f;
int iHapticStrMin = 10;
int iHapticStrMax = 50;
float fHapticMulti = 1.0f;
int iHapticLengthMicroSec = 100000;  // 100 ms
bool bSparkForBeast = true; 

// Experience
float fExpBlock = 3.0f; // At level 20, block needs [86] exp to reach 21. The number 86 increases non-linearly as the skill levels up
float fExpOneHand = 2.0f; // At level 20, one-handed needs [110] exp to reach 21.
float fExpTwoHand = 2.0f; // [179]
float fExpHandToHand = 2.0f; // [7] in vanilla, but this number may have been changed by Hand To Hand

int64_t collisionIgnoreDur = 60;
int64_t collisionEffectDurEnemyShort = 30;  // Within 30 frames, any attack from the enemy is nullified
int64_t collisionEffectDurEnemyLong =
    90;  // Within 90 frames, only the first attack from enemy will be nullified
         // Is there any attack animation whose start and hit will be longer than 90 frames?
int64_t iDelayEnemyHit = 8;
float fMagicNum1 = -0.3f;
float fMagicNum2 = 0.0f;
float fMagicNum3 = -4.3f;


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
    sFeedback.Load(ini);
    sEffectOnEnemy.Load(ini);
    sEffectOnPlayer.Load(ini);
    sExperience.Load(ini);
    sShield.Load(ini);
    sProjectile.Load(ini);
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

    detail::get_value(
        a_ini, bEnableProjParry, section, "EnableProjectileParry",
        "; Set this to false if you want to disable the parry of projectiles (arrows, firebolts). Default:\"true\".");
    
}

void Settings::Difficulty::Load(CSimpleIniA& a_ini) {
    static const char* section = "==========1. Important Settings==========";

    detail::get_value(a_ini, bPlayerMustBeAttacking, section, "PlayerMustBeAttacking",
        ";======[1.1] Parry difficulty\n"
        ";\n"
        "; When this is \"true\", player must be attacking to trigger a parry.\n"
        "; ===If you are a SE or AE player, I am setting this to \"true\" while setting CollisionDistance to a higher value.\n"
        ";         Otherwise the parry is annoying.\n"
        "; ===If you are a VR player, I am setting this to \"false\" while setting CollisionDistance to a lower value.\n"
        ";         Otherwise you can't parry naturally. \n"
        "; Default:\"true\" for SE or AE player, \"false\" for VR player");

    detail::get_value(
        a_ini, fCollisionDistThres, section, "CollisionDistance",
        "; To trigger a collision between two weapons, how close weapons should be.\n"
        "; Higher value means easier parry. Recommend less than 30.0 for VR players, and 30.0~50.0 for SE and AE players. Unit: around 1.4 centimeter. Default:\"15.0\" for VR, \"50.0\" for SE/AE");

    detail::get_value(a_ini, fRangeMulti, section, "WeaponRangeMultiply",
        "; The length of weapon is multiplied by this value. This length is only used by this mod.\n"
        "; Higher value means easier parry. \"1.0\" matches the models of most weapons pretty good. Default:\"1.0\" for VR, \"1.35\" for SE/AE");

    detail::get_value(a_ini, fProjCollisionDistThres, section, "ProjectileParryDistance",
        "; To trigger a parry to projectile (spell, arrow), how close weapon and projectile should be.\n"
        "; Higher value means easier parry. Recommend less than 20.0 for VR players, and around 80 for "
        "SE and AE players. Unit: around 1.4 centimeter. Default:\"12.0\" for VR, \"60.0\" for SE/AE");

    
    detail::get_value(
        a_ini, fPlayerStaUnableParryThresPer, section, "PlayerUnableToParryThreshold",
        ";======[1.2] Limitations on player\n"
        ";\n"
        "; When player's stamina is below this ratio, they are unable to parry.\n"
        "; Setting this to \"0.0\" makes player always able to parry\n"
        "; Setting this to \"0.1\" makes player unable to parry if their stamina is below 10%, making the combat more engaging and RPG-like\n"
        "; Default: \"0.1\"");

    detail::get_value(a_ini, fPlayerStaStopThresPer, section, "PlayerStopThreshold",
        ";===If you are a VR player, no need to read this one\n"
        "; When player's stamina is below this ratio, they will stop their attack combo.\n"
        "; Setting this to \"0.0\" makes player always able to continue attack. Recommend VR players to set this to \"0.0\"\n"
        "; Default: \"0.3\" for SE or AE player, \"0.0\" for VR player");

    detail::get_value(
        a_ini, fPlayerStaLargeRecoilThresPer, section, "PlayerStaggerThreshold",
        ";===If you are a VR player, no need to read this one\n"
        "; When player's stamina is below this ratio, they will stop their attack combo and stagger.\n"
        "; Setting this to \"0.0\" makes player always not staggered. Recommend VR players to set this to \"0.0\"\n"
        "; Default: \"0.05\" for SE or AE player, \"0.0\" for VR player");

}


void Settings::Feedback::Load(CSimpleIniA& a_ini) {
    static const char* section = "==========2. Parry Feedback==========";

    detail::get_value(a_ini, iSparkSpawn, section, "SpawnSparkFrame",
        "; For how many frames after collision do we continue to spawn additional sparks (collision itself spawns 1 spark). \n"
        "; Spark is spawned every 6 frames. So a number 12 means spawning 2 sparks after parry (3 sparks in total). \n"
        "; If you use vanilla spark, 12 is probably good. If you use mods offering big, bright spark, change this to 6 or even 0. Default:\"6\"");

    detail::get_value(a_ini, bSparkForBeast, section, "SparkForBeast",
        "; If this is false, there won't be spark if player is beast (like werewolf) or enemy is beast. Default:\"true\"");

    detail::get_value(
        a_ini, fHapticMulti, section, "HapticStrengthMultiplier",
        "; haptic_strength_on_controller = Stamina_cost_to_player * HapticStrengthMultiplier.\n"
        "; The final haptic_strength_on_controller is modified to fit in [HapticStrengthMin, HapticStrengthMax].\n"
        ";\n"
        "; As shown above, the actual stamina cost multiplies this value is the haptic strength. Default:\"1.0\"");

    detail::get_value(a_ini, iHapticStrMin, section, "HapticStrengthMin",
                      "; The minimal haptic strength. Default:\"10\"");

    detail::get_value(a_ini, iHapticStrMax, section, "HapticStrengthMax",
                      "; The maximal haptic strength. Default:\"50\"");

    detail::get_value(a_ini, iHapticLengthMicroSec, section, "HapticDuration",
                      "; The haptic duration. After configured here, it won't be changed by game events. Unit: "
                      "microsecond. \"100000\" is 0.1 seconds. Default:\"100000\"");

    detail::get_value(
        a_ini, fTimeSlowRatio, section, "TimeSlowRatio",
        ";===If you are a SE/AE player, no need to read the four settings below, because SpellWheelVR is required\n"
        "; After a parry, time scale will be set to this ratio for a few frames. \n"
        "; \"0.1\" means time flows at 10% of normal speed. Default:\"0.1\"");

    detail::get_value(a_ini, iTimeSlowFrameNormal, section, "TimeSlowFrameNormal",
                      "; After a normal parry, for how many frames should time be slow. \n"
                      "; Default:\"12\"");
    detail::get_value(a_ini, iTimeSlowFrameStop, section, "TimeSlowFrameStop",
                      "; After a parry that stops enemy, for how many frames should time be slow. \n"
                      "; Default:\"25\"");
    detail::get_value(a_ini, iTimeSlowFrameLargeRecoil, section, "TimeSlowFrameStagger",
                      "; After a parry that staggers enemy, for how many frames should time be slow. \n"
                      "; Default:\"50\"");
}

void Settings::EffectOnEnemy::Load(CSimpleIniA& a_ini) {
    static const char* section = "==========3. Parry Effect on Enemies==========";

    detail::get_value(
        a_ini, fEnemyPushVelocityMulti, section, "EnemyPushSpeedMulti",
        ";======[3.1] Hit effects on enemy\n"
        ";\n"
        "; After a parry, this value controls how fast the enemy should be pushed away. This creates a little juice as parry feedback\n"
        "; Higher value means pushing faster farther. A value below 8.0 is often effectless. Default:\"8.0\"");

    detail::get_value(a_ini, fEnemyPushMaxDist, section, "EnemyPushDistance",
        "; After a parry, the max distance the enemy may be pushed away\n"
        "; The actual distance is increased by the speed of your weapon, but it can't exceed this value\n"
        "; Higher value means pushing farther. Unit:~ 1.4 cm. Default:\"15.0\" for SE/AE, \"35.0\" for VR");

    detail::get_value(
        a_ini, fEnemyRotStep, section, "EnemyRotationMultiplier",
        "; After a parry, how fast the enemy should rotate. This creates a little juice as parry feedback.\n The rotation direction and duration is determined by the "
        "speed of your weapon.\n"
        "; Higher value means faster and more obvious rotation. Unit:~ 51 degrees per second. Default:\"0.15\"");

    detail::get_value(
        a_ini, fEnemyHealthRagdollThresPer, section, "EnemyRagdollHealthThreshold",
        ";===If you are a SE/AE player, no need to read EnemyRagdollHealthThreshold, because PLANCK is required\n"
        "; After a parry that makes enemy stagger (see below), when enemy health is below this percent, they are ragdolled\n"
        "; Higher value means easier ragdoll. \"0.25\" means below 25% of Health. Default:\"0.25\" for VR");

    detail::get_value(a_ini, fEnemyStopVelocityThres, section, "EnemyStopVelocityThreshold",
        ";======[3.2] Enemy stagger based on player weapon speed (recommended to VR)\n"
        ";\n"
        "; When player's weapon speed is above this, enemy will stop the current attack (but they can start a new attack immediately).\n"
        "; \"40.0\" is a really easy-to-reach threshold, like swinging your controller gently. \n"
        "; Unit:~ 1.5 cm per second. Default:\"9999.0\" (stop is not based on speed) for AE or SE player, \"40.0\" for VR player.");

    detail::get_value(a_ini, fEnemyLargeRecoilVelocityThres, section, "EnemyStaggerVelocityThreshold",
        "; When player's weapon speed is above this, enemy will stop the current attack and stagger.\n"
        "; Unit:~ 1.5 cm per second. Default:\"9999.0\" (stagger is not based on speed) for AE and SE player, \"130.0\" for VR player.");

    detail::get_value(a_ini, fEnemyStaStopThresPer, section, "EnemyStopStaminaThreshold",
        ";======[3.3] Enemy stagger based on stamina (recommended to SE/AE)\n"
        ";\n"
        "; When enemy's stamina is below this ratio, they will stop their attack (but they can start a new attack immediately). Set this to "
        "\"1.0\" to always stop them\n"
        "; Set this to \"0.3\" to stop them when their stamina is below 30%.\n"
        "; Default:\"0.3\" for SE and AE player, \"0.0\" (stop is not based on stamina) for VR player");

    detail::get_value(
        a_ini, fEnemyStaLargeRecoilThresPer, section, "EnemyStaggerStaminaThreshold",
        "; When enemy's stamina is below this ratio, they will stop their attack and stagger. Set this to "
        "\"1.0\" to always stop and stagger them\n"
        "; Set this to \"0.05\" to stop them when their stamina is below 5%.\n"
        "; Default:\"0.05\" for SE and AE player, \"0.0\" (stagger is not based on stamina) for VR player");

    
    detail::get_value(
        a_ini, fEnemyStaCostWeapMulti, section, "EnemyStaminaCostMultiplier",
        ";======[3.4] Stamina cost to enemy\n"
        ";\n"
        "; Stamina_cost_to_enemy = EnemyStaminaCostMultiplier * (player_weapon_damage * "
        "player_related_skill *  2.0 - enemy_weapon_damage * enemy_related_skill)\n"
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
    static const char* section = "==========4. Parry Effect on Player==========";

    detail::get_value(
        a_ini, bPlayerCheckHeavyGauntlet, section, "HandToHandPlayerMustWearHeavyArmor",
        "; If this is false, whenever player wields two unarmed hands, they can parry with hands.\n"
        "; If this is true, when player wields two unarmed hands, they must wear heavy armor to be able to parry. Default:\"false\"");

    detail::get_value(a_ini, fPlayerPushVelocityMulti, section, "PlayerPushSpeedMulti",
        ";======[4.1] Hit effect on player\n"
        ";\n"
        "; After a parry, this value controls how fast the player should be pushed away. This creates a "
        "little juice as parry feedback\n"
        "; Higher value means pushing faster. A value below 8.0 is often effectless. Default:\"8.0\"");

    detail::get_value(a_ini, fPlayerPushMaxDist, section, "PlayerPushDistance",
        "; After a parry, the max distance the player may be pushed away\n"
        "; The actual distance is decreased by the speed of you weapon. You can avoid being pushed by swinging weapon during parry\n"
        "; Higher value means pushing farther. Unit:~ 1.4 cm. Default:\"35.0\" for SE/AE, \"60.0\" for VR");

    detail::get_value(
        a_ini, fPlayerStaCostWeapMulti, section, "PlayerStaminaCostMultiplier",
        ";======[4.2] Stamina cost to player\n"
        ";\n"
        "; Stamina_cost_to_player = PlayerStaminaCostMultiplier * (enemy_weapon_damage * "
        "enemy_related_skill * 2.0 - player_weapon_damage * player_related_skill)\n"
        "; Stamina_cost_to_player is doubled if enemy is power attacking and player is not\n"
        "; The final Stamina_cost_to_player will be modified to fit in [PlayerMinStaminaCost, PlayerMaxStaminaCost]\n"
        ";\n"
        "; After all modification, VR player can reduce their actual stamina cost by a mechanism described in \"Weapon speed reward\" below\n"
        "; SE and AE players already have their stamina cost reduced, compared to VR players\n"
        ";\n"
        "; As shown in the formula above, the stamina cost to player is multiplied by this value. Default:\"0.3\" for SE/AE, \"1.0\" for VR");

    detail::get_value(a_ini, fPlayerStaCostMin, section, "PlayerMinStaminaCost",
                      "; The minimal stamina cost to the player for each parry. Default:\"3.0\" for SE/AE, \"10.0\" for VR");

    detail::get_value(a_ini, fPlayerStaCostMax, section, "PlayerMaxStaminaCost",
                      "; The maximal stamina cost to the player for each parry. Default:\"20.0\" for SE/AE, \"40.0\" for VR");

    detail::get_value(
        a_ini, fPlayerWeaponSpeedRewardThres, section, "PlayerWeaponSpeedRewardThreshold1",
        ";======[4.3] Weapon speed reward VR\n"
        ";\n"
        "; === If you are a SE/AE player, no need to read the four settings below.\n"
        "; Because SE/AE don't have controller, so stamina cost is already reduced for SE/AE players, and settings below are turned off\n"
        "; === If you are a VR player, your base stamina cost is higher, but when you move weapon at some speed when parry, you can reduce the stamina cost\n"
        "; Stamina_cost_to_player is multiplied by PlayerWeaponSpeedReward1, if your weapon speed is higher than PlayerWeaponSpeedRewardThreshold1\n"
        "; If not passing Threshold1, Stamina_cost_to_player is multiplied by PlayerWeaponSpeedReward2, if your weapon speed is higher than PlayerWeaponSpeedRewardThreshold2\n"
        ";\n"
        "; When player is swinging their weapon pretty fast, they reduce their stamina cost by a lot.\n"
        "; \"150.0\" is about swinging your controller to attack. Unit:~ 1.5 cm per second. Default:\"150.0\"");

    detail::get_value(a_ini, fPlayerWeaponSpeedReward, section, "PlayerWeaponSpeedReward1",
        "; The Stamina_cost_to_player is multiplied by this value, if passing PlayerWeaponSpeedRewardThreshold1.\n"
        "; \"0.35\" means the stamina cost is reduced by 65%. Default:\"0.35\"");

    detail::get_value(
        a_ini, fPlayerWeaponSpeedRewardThres2, section, "PlayerWeaponSpeedRewardThreshold2",
        "; This takes effect when PlayerWeaponSpeedRewardThreshold1 is not passed."
        "; When player is at least moving their weapon instead of holding it still, they reduce their stamina cost.\n"
        "; \"30.0\" is very easy to pass. Unit:~ 1.5 cm per second. Default:\"30.0\"");

    detail::get_value(
        a_ini, fPlayerWeaponSpeedReward2, section, "PlayerWeaponSpeedReward2",
        "; The Stamina_cost_to_player is multiplied by this value, if passing PlayerWeaponSpeedRewardThreshold2.\n"
        "; \"0.70\" means the stamina cost is reduced by 30%. Default:\"0.70\"");
}

void Settings::Experience::Load(CSimpleIniA& a_ini) {
    static const char* section = "==========5. Player Experience Gain==========";

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

void Settings::Shield::Load(CSimpleIniA& a_ini) {
    static const char* section = "==========6. Shield Collision==========";

    detail::get_value(a_ini, bEnableShieldCollision, section, "EnableShieldCollision",
            "; Whether this mod calculates collision between player's shield and enemy's weapons and projectiles. Default:\"true\"");
    detail::get_value(a_ini, bTreatShieldAsParry, section, "TreatShieldAsParry",
            "; If true, shield will be handled as if it's a weapon, triggering the parry system of this mod.\n"
            "; If false, shield will be different: it triggers vanilla blocking system and other blocking mods. Default:\"false\"");
    detail::get_value(a_ini, fShieldRadius, section, "ShieldRadius",
            "; Radius of shield's collision. If you feel the collision doesn't match the shield's model, adjust this. Default:\"40.0\"");
    detail::get_value(a_ini, iFrameBlockDur, section, "BlockDurationFrame",
            "; If TreatShieldAsParry is false, this value takes effect.\n"
            "; When enemy's weapon touches player's shield, player will start blocking pose for a few frames and stop.\n"
            "; This is the number of frames. If this value doesn't exist, player will be stuck in blocking pose.\n"
            "; If you ever get stuck in blocking pose, you can jump to stop it. Welcome to report a bug to me!\n"
            "; Default:\"20\"");
}


void Settings::Projectile::Load(CSimpleIniA& a_ini) {
    static const char* section = "==========7. Projectile Parry==========";

    
    detail::get_value(a_ini, fProjLength, section, "ProjectileLength",
        "; The length of each projectile. Higher value means easier parry. Default:\"50.0\"");

    detail::get_value(a_ini, iProjCollisionFrame, section, "ProjectileParryFrame",
        "; When calculating the parry of projectile, this mod also considers player's weapon position in "
        "the last ProjectileParryFrame frames.\n"
        "; This enables you to parry like how Jedi deflects blaster shots by swinging lightsaber. \n"
        "; Higher value means easier parry, but don't make it too high. Default:\"5\"");

    detail::get_value(a_ini, fProjSlowRadius, section, "ProjectileSlowRadius",
        "; You can SLOW DOWN projectiles like Neo in The Matrix using your magicka!!!"
        "; A projectile will be slowed down for a few frames if:\n"
        "; (1) Player has more than SlowMagickaCost\n"
        "; (2) Player is pressing the trigger on VR controller\n"
        "; \n"
        "; \n"
        "; This is the radius of the slow down effect. Don't recommend too large or too small values. Default:\"250.0\"");

    detail::get_value(a_ini, iProjSlowButton1, section, "ProjectileSlowButton1",
        "; The code of the button you need to press to slow projectiles. For VR, 33 is the trigger button. \n"
        "; For SE/AE, 4096 is A on Xbox controller, and 18 is E on keyboard. \n"
        "; If those values don't work for you, you can find the code of the button you want by these steps:\n"
        "; (1) Set TraceLevel to 0; (2) Open the trace file (location is in TraceLevel section) when you play\n"
        "; (3) Hold the button you want during combat; (4) In the trace file, you will see a lot of \"... Pressed key XXX\"\n"
        "; (5) Come back here, set ProjectileSlowButton to the XXX you see, save edits"
        "; Default:\"33\" for VR, \"18\" for SE/AE");

    detail::get_value(
        a_ini, iProjSlowButton2, section, "ProjectileSlowButton2",
        "; Another button you can press to slow projectiles.  Default:\"33\" for VR, \"4096\" for SE/AE");

    detail::get_value(a_ini, fProjSlowCost, section, "ProjectileSlowMagickaCost",
        "; The magicka cost to slow down the projectile.\n"
        "; If not enough, the projectile won't be slowed. Default:\"10.0\"");

    detail::get_value(a_ini, fProjSlowRatio, section, "ProjectileSlowRatio",
        "; When slowed down, projectile_new_speed = projectile_original_speed * ProjectileSlowRatio.\n"
        "; Lower value means slower speed. Default:\"0.17\"");

    detail::get_value(a_ini, iProjSlowFrame, section, "ProjectileSlowFrame",
        "; How many frames will the projectile be slowed.\n"
        "; Default:\"60\"");
}




void Settings::Technique::Load(CSimpleIniA& a_ini) {
    static const char* section = "==========Other Settings==========";

    detail::get_value(
        a_ini, iTraceLevel, section, "TraceLevel",
        "; ===WARNING: normally you don't need to change any value below. Please read instructions carefully before any "
        "change.\n"
        ";\n"
        "; Set to 2 to only have info. 1 to turn on debug. 0 to turn on trace. Printing to C:\\Users\\XXX\\Documents\\My Games\\Skyrim VR\\SKSE\\WeaponCollision.log\n"
        "; In the worst case, turning this on makes the file grow at 10MB/minute, and might slow the game down (very unlikely), but useful when you report bugs to me. Default:\"2\"");


    detail::get_value(a_ini, fDetectEnemy, section, "EnemyDetectionRange",
        "; We only calculate weapon collisions between player and enemies in this range, for optimization.\n"
        "; Unit:~ 1.5 cm. Default:\"600.0\"");

    detail::get_value(a_ini, bShowPlayerWeaponSegment, section, "DebugShowPlayerWeaponRange",
        "; Display some effect on the two ends of player's weapons.\n"
                      "; I turn this on to verify if the weapon length used by this mod is correct. Default:\"false\"");
    
    detail::get_value(a_ini, bShowEnemyWeaponSegment, section, "DebugShowEnemyWeaponRange",
                      "; Display some effect on the two ends of enemy's weapons.\n"
                      "; I turn this on to verify if the weapon length used by this mod is correct. Default:\"false\"");

    //detail::get_value(
    //    a_ini, fMagicNum1, section, "DebugTuneAngle1",
    //    "; ===No need to read this option\n"
    //    "; A number I use to debug weapon angle of special enemies who don't have hands. Default:\"0.0\"");
    //detail::get_value(
    //    a_ini, fMagicNum2, section, "DebugTuneAngle2",
    //    "; ===No need to read this option\n"
    //    "; A number I use to debug weapon angle of special enemies who don't have hands. Default:\"0.0\"");
    //detail::get_value(
    //    a_ini, fMagicNum3, section, "DebugTuneAngle3",
    //    "; ===No need to read this option\n"
    //    "; A number I use to debug weapon angle of special enemies who don't have hands. Default:\"0.0\"");

    detail::get_value(
        a_ini, collisionIgnoreDur, section, "collisionIgnoreDur",
        "; After a collision, for how many frames should we ignore following collisions of the same enemy.\n"
        "; A value too low creates lots of collision for each parry. Default:\"60\"");

    detail::get_value(
        a_ini, collisionEffectDurEnemyShort, section, "NullifyEnemyAllAttackFrames",
        "; After a collision, within how many frames should we nullify all melee hits from this enemy.\n"
        "; Default:\"30\"");

    detail::get_value(
        a_ini, collisionEffectDurEnemyLong, section, "NullifyEnemyFirstAttackFrames",
        "; After a collision, within how many frames should we nullify the first melee hit from this enemy.\n"
        "; Default:\"90\"");

    //detail::get_value(
    //    a_ini, fProjGravity, section, "ProjectileGravity",
    //    "; When projectile is slowed, this is used to compensate gravity so it doesn't drop rapidly.\n"
    //    "; Default:\"10\"");

    // Deprecated: no longer used
    //detail::get_value(a_ini, bSwitchHiggsCollision, section, "SwitchHiggsWeaponCollision",
    //    "; If you set EnableWeaponCollision to be 0 in higgs_vr.ini on purpose, you may want to set this to false.\n"
    //    "; Otherwise, please leave this to be true, since this can make projectiles parry less buggy. Default:\"true\"");

    detail::get_value(
        a_ini, iDelayEnemyHit, section, "DelayEnemyHitOnPlayer",
        "; This one is tricky but important. This mod delays melee hit events from enemy to player,\n"
        "; because in Skyrim enemies have some cheating attack animations, where they hit you a few frames before their weapon reaches you\n"
        "; These attack animations are unfairly hard to parry. A delay for about 6 to 9 frames is nearly unnoticeable and fixes this problem.\n"
        "; If your framerate is below 60fps, set it to 5 or so. Let me know if this feature causes any trouble to other mods.\n"
         "; Setting to 0 can disable this feature, meaning enemy hit works as vanilla game, which should make potentially incompatible mods work well.\n"
        "; Default:\"8\"");
}
    
spdlog::level::level_enum TraceLevel(int level) {
    switch (iTraceLevel) {
        case 0:
            return spdlog::level::trace;
        case 1:
            return spdlog::level::debug;
        case 2:
            return spdlog::level::info;
        case 3:
            return spdlog::level::warn;
        case 4:
            return spdlog::level::err;
    }
    return spdlog::level::info;
}