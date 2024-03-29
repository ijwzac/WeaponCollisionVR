#include "Settings.h"

using namespace SKSE;
using namespace SKSE::log;

// Global
bool bEnableWholeMod = true;
bool bHandToHandLoad = false;
bool bPlanck = false;
float fRangeMulti = 1.0f;   
float fCollisionDistThres = 15.0f;
float fCollisionSpeedThres = 0.0f;
float fDetectEnemy = 600.f;
bool bShowPlayerWeaponSegment = false;
bool bShowEnemyWeaponSegment = false;
int64_t iFrameCount = 0;
int64_t iFrameTriggerPress = 0;
int64_t iFrameSlowCost = 0;
int iTraceLevel = 2;
bool bPlayerMustBeAttacking = false;
int64_t iSparkSpawn = 6;
std::chrono::steady_clock::time_point last_time;

// Block
int64_t iFrameStopBlock = 0;
bool bEnableShieldCollision = true;
float fShieldCollisionDist = 12.0f;
int64_t iFrameBlockDur = 30;
float fShieldRadius = 33.0f;
float fOriginCone = 35.0f;
int64_t iFrameSetCone = 0;
bool bPressButtonToBlock = false;
uint32_t iBlockButton = 33;
int64_t iFramePressBlockButton = 0;
float fBlockEnemyLargeRecoilVelocityThres = 100.0f;

// Projectile Parry
bool bEnableProjParry = true;
bool bMagicProjParry = true;
float fProjDetectRange = 800.0f;  // The range of projectile detection
float fProjCollisionDistThres = 12.0f;         // If weapon and projectile distance is smaller than this number, it's a collision
float fProjLength = 50.0f;
int64_t iProjCollisionFrame = 5;  // Collision is calculated for player's weapon positions for the last X frames
int64_t iTimeSlowFrameProj = 30;
float fProjSlowRatio = 0.25f;
float fProjSlowRadius = 370.0f;
int64_t iProjSlowFrame = 60;
float fProjSlowCost = 10.0f;
uint32_t iProjSlowButton1 = 33;
uint32_t iProjSlowButton2 = 33;
float fProjWeapSpeedThres = 10.0f;
float fProjShieldSpeedThres = 14.0f;

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
float fPlayerPushMaxDist = 35.0f;
int64_t collisionEffectDurPlayerShort = 20;
float fPlayerStaCostMin = 12.0f;
float fPlayerStaCostMax = 40.0f;
float fPlayerStaCostWeapMulti = 1.0f;
float fPlayerWeaponSpeedReward = 0.35f;
float fPlayerWeaponSpeedReward2 = 0.7f;
float fPlayerStaUnableParryThresPer = 0.1f;
float fPlayerStaStopThresPer = 0.0f;
float fPlayerStaLargeRecoilThresPer = 0.0f;
bool bPlayerCheckHeavyGauntlet = false;

float fPlayerHealthCostMax = 20.0f;
float fPlayerHealthCostMulti = 1.0f;

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
bool bSparkForFistBowAndStaff = true;

// Experience
float fExpBlock = 3.0f; // At level 20, block needs [86] exp to reach 21. The number 86 increases non-linearly as the skill levels up
float fExpOneHand = 2.0f; // At level 20, one-handed needs [110] exp to reach 21.
float fExpTwoHand = 2.0f; // [179]
float fExpHandToHand = 2.0f; // [7] in vanilla, but this number may have been changed by Hand To Hand

int64_t collisionIgnoreDur = 100;
int64_t collisionEffectDurEnemyShort = 30;  // Within 30 frames, any attack from the enemy is nullified
int64_t collisionEffectDurEnemyLong =
    90;  // Within 90 frames, only the first attack from enemy will be nullified
         // Is there any attack animation whose start and hit will be longer than 90 frames?
int64_t iDelayEnemyHit = 6;
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
    sBlock.Load(ini);
    sProjectile.Load(ini);
    sTechnique.Load(ini);

    err = ini.SaveFile(path);
}

void Settings::Main::Load(CSimpleIniA& a_ini) { 
    static const char* section = "==========Main==========";

    detail::get_value(
        a_ini, bEnableWholeMod, section, "EnableWholeMod",
        ";;;;;;;;;;;;;;;;;;;;;;;;\n; While playing the game, you can DYNAMICALLY CHANGE SETTINGS below. Steps:\n"
        ";(1) Edit settings; (2) Save and close this file; (3) When not in combat, open Skyrim console by \"`\"\n"
        ";(4) Close the console, no need to type anything; (5) Now settings are updated.\n;;;;;;;;;;;;;;;;;;;;;;;;\n;\n;\n"
        "; Set this to false if you want to completely disable this mod. Default:\"true\".");

    detail::get_value(
        a_ini, bEnableProjParry, section, "EnableProjectileParry",
        "; Set this to false if you want to disable the parry of projectiles (arrows, firebolts, etc.). More settings in \"Projectile Parry\". \n"
        "; ===Limitation: if the shooter is too far away (like 5 floors above you), their projectile won't be detected\n"
        "; Default:\"true\".");

    
    detail::get_value(a_ini, bEnableShieldCollision, section, "EnableShieldCollision",
        "; Whether this mod calculates collision between player's shield and enemy's weapons/projectiles. More settings in \"Block\".\n"
        "; Default:\"true\"");
    
}

void Settings::Difficulty::Load(CSimpleIniA& a_ini) {
    static const char* section = "==========1. Important Weapon Parry Settings==========";

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
        a_ini, fCollisionSpeedThres, section, "WeaponCollisionSpeed",
        "; To trigger a collision between two weapons, player's weapon speed must be larger than this value. Doesn't affect shield collision.\n"
                      "; Smaller value means easier parry. If you feel it's too easy to trigger parry in VR, set "
                      "this to values like 15.0.\n"
        "; Unit: around 1.4 centimeter/second. Default:\"0.0\" (the speed is not checked)");


    detail::get_value(
        a_ini, fCollisionDistThres, section, "WeaponCollisionDistance",
        "; To trigger a collision between two weapons, how close weapons should be.\n"
        "; Higher value means easier parry. Recommend less than 30.0 for VR players, and 30.0~50.0 for SE and AE players. Unit: around 1.4 centimeter. Default:\"15.0\" for VR, \"50.0\" for SE/AE");


    detail::get_value(a_ini, fRangeMulti, section, "WeaponRangeMultiply",
        "; The length of weapon is multiplied by this value. This length is only used by this mod.\n"
        "; Higher value means easier parry. \"1.0\" matches the models of most weapons pretty good. Default:\"1.0\" for VR, \"1.35\" for SE/AE");


    detail::get_value(a_ini, iDelayEnemyHit, section, "DelayEnemyHitOnPlayer",
        "; This one is tricky but important to difficulty. This mod delays melee hit events from enemy to player,\n"
        "; because in Skyrim enemies have some cheating attack animations, where they hit you a few "
        "frames before their weapon reaches you\n"
        "; These attack animations are unfairly hard to parry. A delay for 6 to 9 frames can fix this problem.\n"
        "; However, if too high, player can parry an attack even if enemy's sword cut through their body a few frames ago.\n"
        "; If you installed mod Retimed Hit Frames, I suggest you set this to 3 or 4.\n"
        "; If you have some really good attack animation mods with precise hit frame, probably this number can be set lower.\n"
        "; Let me know if this feature causes any trouble to other mods.\n"
        "; Setting to 0 can disable this feature, meaning enemy hit works as vanilla game, which should "
        "make potentially incompatible mods work well.\n"
        "; Default:\"6\"");

    
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

    detail::get_value(a_ini, iSparkSpawn, section, "SpawnExtraSparkFrame",
        "; For how many frames after collision do we continue to spawn extra sparks (collision itself spawns 1 spark). \n"
        "; Spark is spawned every 6 frames. So a number 12 means spawning 2 sparks after parry (3 sparks in total). \n"
        "; If you use mods offering big and bright sparks, use 6 or even 0 (0 still has 1 spark). Default:\"6\"");

    detail::get_value(a_ini, bSparkForBeast, section, "SparkForBeast",
        "; If this is false, there won't be spark if enemy is beast or player is beast (like werewolf). Default:\"true\"");

    detail::get_value(a_ini, bSparkForFistBowAndStaff, section, "SparkForFistBowAndStaff",
        "; If this is false, there won't be spark if player is using fists, bow or staff to parry. "
        "Default:\"true\"");

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
        ";======[3.1] Hit pushback on enemy\n"
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
        a_ini, fPlayerHealthCostMulti, section, "PlayerHealthCostMulti",
        ";=========ATTENTION! [4.1] is new in Version 1.0.0\n"
        ";\n"
        ";======[4.1] Damage if holding weapon still\n"
        "; Many players complain this mod makes their combat too easy, so now we have this new section.\n"
        "; If you don't like it, set PlayerHealthCostMulti to 0.0\n"
        ";\n"
        "; If player's weapon speed is lower than EnemyStopVelocityThreshold, they get damage and their stamina is reduced a lot (see [4.3]).\n"
        "; The damage depends on enemy's weapon, skill, power attack, player's armor rate, player's Block skill.\n"
        "; The damage also depends on player's weapon speed. When speed equals to or above EnemyStopVelocityThreshold, damage is 0.\n"
        "; The damage is also multiplied by PlayerHealthCostMulti.\n"
        "; Higher value means higher damage and more challenging. Default:\"1.0\"");

    detail::get_value(
        a_ini, fPlayerHealthCostMax, section, "PlayerHealthCostMax",
        "; The damage to player is capped by PlayerHealthCostMax. Default:\"20.0\"");


    detail::get_value(a_ini, fPlayerPushVelocityMulti, section, "PlayerPushSpeedMulti",
        ";======[4.2] Hit pushback on player\n"
        ";\n"
        "; After a parry, this value controls how fast the player should be pushed away. This creates a "
        "little juice as parry feedback\n"
        "; Higher value means pushing faster. A value below 8.0 is often effectless. Default:\"8.0\"");

    detail::get_value(a_ini, fPlayerPushMaxDist, section, "PlayerPushDistance",
        "; After a parry, the max distance the player may be pushed away\n"
        "; The actual distance is decreased by the speed of you weapon. You can avoid being pushed by swinging weapon during parry\n"
        "; Higher value means pushing farther. Unit:~ 1.4 cm. Default:\"35.0\"");

    
    detail::get_value(a_ini, bPlayerCheckHeavyGauntlet, section, "HandToHandPlayerMustWearHeavyArmor",
                      "; If this is false, whenever player wields two unarmed hands, they can parry with hands.\n"
                      "; If this is true, when player wields two unarmed hands, they must wear heavy armor to be able "
                      "to parry. Default:\"false\"");

    detail::get_value(
        a_ini, fPlayerStaCostWeapMulti, section, "PlayerStaminaCostMultiplier",
        ";======[4.3] Stamina cost to player\n"
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


    detail::get_value(a_ini, fPlayerWeaponSpeedReward, section, "PlayerWeaponSpeedReward1",
        ";======[4.4] Stamina cost reduced when weapon speed is high (VR)\n"
        ";\n"
        "; === If you are a SE/AE player, no need to read the four settings below.\n"
        "; Because SE/AE don't have VR controller, so stamina cost is already reduced for SE/AE players, and settings "
        "below are turned off\n"
        "; === If you are a VR player, your base stamina cost is higher, but when you move weapon at some speed when "
        "parry, you can reduce the stamina cost\n"
        "; Stamina_cost_to_player is multiplied by PlayerWeaponSpeedReward1, if your weapon speed is higher than "
        "EnemyStaggerVelocityThreshold\n"
        "; If not passing EnemyStaggerVelocityThreshold, Stamina_cost_to_player is multiplied by PlayerWeaponSpeedReward2, if your weapon "
        "speed is higher than EnemyStopVelocityThreshold\n"
        ";\n"
        "; When player is swinging their weapon pretty fast, they reduce their stamina cost by a lot. See formula above\n"
        "; \"0.35\" means the stamina cost is reduced by 65%. Default:\"0.35\"");

    detail::get_value(
        a_ini, fPlayerWeaponSpeedReward2, section, "PlayerWeaponSpeedReward2",
        "; When player is at least moving their weapon instead of holding it still, they reduce their stamina cost. See formula above\n"
        "; \"0.70\" means the stamina cost is reduced by 30%. Default:\"0.70\"");
}

void Settings::Experience::Load(CSimpleIniA& a_ini) {
    static const char* section = "==========5. Player Experience Gain==========";

    detail::get_value(a_ini, fExpBlock, section, "BlockExperienceGain",
                      "; Each block with shield (in this mod) gives player this amount of block experience. Default:\"3.0\"");
    detail::get_value(a_ini, fExpOneHand, section, "OneHandedExperienceGain",
                      "; Each parry gives player this amount of one-handed experience, if parrying with one-handed weapon. Default:\"2.0\"");
    detail::get_value(a_ini, fExpTwoHand, section, "TwoHandedExperienceGain",
                      "; Each parry gives player this amount of two-handed experience, if parrying with two-handed weapon. Default:\"2.0\"");
    detail::get_value(a_ini, fExpHandToHand, section, "HandToHandExperienceGain",
                      "; If Hand to Hand - An Adamant Addon and Adamant are installed, and player is parrying with fists\n"
        "; (VR players should install Hand to Hand - An Adamant VR Addon and the base mod) \n"
        "; Each parry gives player this amount of hand-to-hand experience. Default:\"2.0\"");
}

void Settings::Block::Load(CSimpleIniA& a_ini) {
    static const char* section = "==========6. Block==========";

    
    detail::get_value(a_ini, fShieldCollisionDist, section, "ShieldCollisionDistance",
                      "; To trigger a collision between player's shield and enemy's weapon, how close they should be.\n"
                      "; Higher value means easier parry. Since vanilla shield blocking is only a problem in VR, this "
                      "is turned off in SE/AE\n"
                      "; Unit: around 1.4 centimeter. Default:\"12.0\" for VR, \"0.0\" for SE/AE");



    detail::get_value(a_ini, fShieldRadius, section, "ShieldRadius",
        "; Radius of shield's collision. In this mod, all shields are treated as a plate, sharing the same radius.\n"
        "; If you feel the collision doesn't match the shield's model, adjust this. Default:\"33.0\"");

    detail::get_value(
        a_ini, iFrameBlockDur, section, "BlockDurationFrame",
        "; When enemy's weapon touches player's shield, player will start blocking pose for a few frames and stop.\n"
        "; This is the number of frames. If this value is too high, player will be stuck in blocking pose for longer.\n"
        "; If you ever get stuck in blocking pose, you can jump to stop it. Welcome to report a bug to me!\n"
        "; Default:\"30\"");

    detail::get_value(a_ini, fBlockEnemyLargeRecoilVelocityThres, section, "BlockStaggerVelocityThreshold",
        "; When player's shield speed is above this, enemy will stagger, pushed back more, and have timeslow effect.\n"
        "; Unit:~ 1.5 cm per second. \"100.0\"");

    detail::get_value(
        a_ini, bPressButtonToBlock, section, "PressButtonToBlock",
        "; ==== If you use any timed-block mods like SOSCBO or Deflection, you may want to read this\n"
        "; When this is false, this mod makes you block enemy's attack if your shield collides with enemy's weapon.\n"
        "; When this is true, this mod checks both the collision and whether you are holding BlockButton.\n"
        "; This feature is designed for those timed-block mods. After enable this, there are 4 cases:\n"
        "; (1) To have a timed-block, you need to press the button right before being hit and \n"
        "; also make your shield collide with enemy's weapon. The exact timing is decided by those mods.\n"
        "; (2) If you hold the button long before hit, those timed-block mods won't consider this as a timed-block\n"
        "; (3) If you hold the button but doesn't make shield collide with enemy's weapon, the block won't happen\n"
        "; (4) If you make shield collide with enemy's weapon but don't hold the button, the block won't happen\n"
        "; IMO, this makes the blocking more challenging and fun with SOSCBO or Deflection. \n"
        "; If you don't enable this, those mods will treat almost every block as a timed-block\n"
        "; My settings in SOSCBO: Timed-block cooldown: 0.00 sec (important)\n"
        ";      Timed-block window: 0.15 sec (yes, now you can use a very low value because block "
        "with this mod feels more accurate)\n"
        "; Default:\"false\"");

    detail::get_value(
        a_ini, iBlockButton, section, "BlockButton",
        "; ==== If you use any timed-block mods like SOSCBO or Deflection, you may want to set this\n"
        "; When PressButtonToBlock is true, this is the code of that block button. 2 is grip, 33 is trigger. Both hands are detected\n"
        "; If prefer other buttons, you can find button code by this mod. See instructions below ProjectileSlowButton1\n"
        "; Default:\"33\"");

    detail::get_value(
        a_ini, fOriginCone, section, "Origin_fCombatHitConeAngle",
        "; ==== Most players can ignore this one\n"
        "; When this mod makes player block, it also changes fCombatHitConeAngle to 180.0\n"
        "; fCombatHitConeAngle is a hidden setting in Skyrim. Two actors' facing angle must be smaller than this to trigger block.\n"
        "; To make player block more freely, this mod needs to set fCombatHitConeAngle to 180.0 \n"
        "; When player exits combat, this mod will set fCombatHitConeAngle to Origin_fCombatHitConeAngle \n"
        "; Default:\"35.0\"");
}


void Settings::Projectile::Load(CSimpleIniA& a_ini) {
    static const char* section = "==========7. Projectile Parry==========";
    detail::get_value(a_ini, bMagicProjParry, section, "EnableMagicProjectileParry",
        "; If this is false, you are not able to parry magic projectiles like fireballs using weapons. You are always able to parry them using shields\n"
        "; Default:\"true\"");

    
    detail::get_value(a_ini, fProjCollisionDistThres, section, "ProjectileParryDistance",
        "; To trigger a parry to projectile (spell, arrow), how close weapon and projectile should be.\n"
        "; Higher value means easier parry. Recommend less than 20.0 for VR players, and around 60 for "
        "SE and AE players. Unit: around 1.4 centimeter. Default:\"12.0\" for VR, \"60.0\" for SE/AE");
    
    detail::get_value(a_ini, fProjLength, section, "ProjectileLength",
        "; The length of each projectile (including arrow, fireball, icespike, spiderweb, etc.) \n"
        "; BTW, parried arrows can't be picked up, to prevent a CTD bug\n"
        "; Higher value means easier parry. Default:\"50.0\"");

    detail::get_value(a_ini, iProjCollisionFrame, section, "ProjectileParryFrame",
        "; When calculating the parry of projectile, this mod also considers player's weapon position in "
        "the last ProjectileParryFrame frames.\n"
        "; This makes parry more smooth, and also enables you to parry like how Jedi deflects blaster shots by swinging lightsaber. \n"
        "; Higher value means easier parry, but don't make it too high. Default:\"5\"");

    detail::get_value(a_ini, fProjWeapSpeedThres, section, "ProjectileParryWeaponMinSpeed",
         "; To be able to parry projectiles, weapon's speed must be higher than this. \n"
        "; 10.0 is really easy to achieve. Default:\"10.0\"");

    detail::get_value(a_ini, fProjShieldSpeedThres, section, "ProjectileParryShieldMinSpeed",
            "; To be able to parry projectiles, shield's speed must be higher than this. \n"
            "; I set this higher than weapon's because it's really easy to pary with shield (its collision is so big). Default:\"14.0\"");

    detail::get_value(a_ini, fProjSlowRadius, section, "ProjectileSlowRadius",
        "; ==== You can SLOW DOWN projectiles like Neo in The Matrix, at the cost of some magicka!!!\n"
        "; A projectile will be slowed down for a few frames if:\n"
        "; (1) Player's magicka is more than SlowMagickaCost\n"
        "; (2) Player is pressing the trigger on VR controller (either one is OK)\n"
        "; \n"
        "; \n"
        "; This is the radius of the slow down effect. Don't recommend too large or too small values. Default:\"370.0\"");

    detail::get_value(a_ini, iProjSlowButton1, section, "ProjectileSlowButton1",
        "; The code of the button you need to press to slow projectiles. For VR, 33 is the trigger button, and 2 is grip. \n"
        "; For SE/AE, 4096 is A on Xbox controller, and 18 is E on keyboard. \n"
        "; If those values don't work for you, you can find the code of the button you want by these steps:\n"
        "; (1) Set TraceLevel to 0; (2) Open the trace (C:\\Users\\XXX\\Documents\\My Games\\Skyrim VR\\SKSE\\WeaponCollision.log) when you play\n"
        "; (3) Hold the button you want during combat; (4) In the trace file, you will see a lot of \"... Pressed key XXX\"\n"
        "; (5) Come back here, set ProjectileSlowButton to the XXX you see, save edits\n"
        "; Default:\"33\" for VR, \"18\" for SE/AE");

    detail::get_value(
        a_ini, iProjSlowButton2, section, "ProjectileSlowButton2",
        "; Another button you can press to slow projectiles. This is mainly for SE/AE.  Default:\"33\" for VR, \"4096\" for SE/AE");

    detail::get_value(a_ini, fProjSlowCost, section, "ProjectileSlowMagickaCost",
        "; The magicka cost to slow down the projectile.\n"
        "; If not enough, the projectile won't be slowed. Default:\"10.0\"");

    detail::get_value(a_ini, fProjSlowRatio, section, "ProjectileSlowRatio",
        "; When slowed down, projectile_new_speed = projectile_original_speed * ProjectileSlowRatio.\n"
        "; Lower value means slower speed. Default:\"0.25\"");

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
        "; In the worst case, setting to 0 makes the file grow at 10MB/minute, and might slow the game down (very unlikely), but useful when you report bugs to me. Default:\"2\"");


    detail::get_value(a_ini, fDetectEnemy, section, "EnemyDetectionRange",
        "; We only calculate weapon collisions between player and enemies in this range, for optimization.\n"
        "; Unit:~ 1.5 cm. Default:\"600.0\"");

    detail::get_value(
        a_ini, fProjDetectRange, section, "ProjectileDetectionRange",
        "; We only calculate projectile parry in this range. Due to some unknown bugs, if this is too small\n"
        "; some projectiles won't be detected even if they are right at your face. Unit:~ 1.5 cm. Default:\"800.0\"");

    detail::get_value(a_ini, bShowPlayerWeaponSegment, section, "DebugShowPlayerWeaponRange",
        "; Display some effect on the two ends of player's weapons (for shield, also the normalized verticle vector).\n"
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
        "; A value too low creates lots of collision for each parry. Default:\"100\"");

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