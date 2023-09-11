#include <stddef.h>
#include "OnMeleeHit.h"
#include "PrecisionAPI.h"
#include "Settings.h"
#include "Utils.h"
#include "OnFrame.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

int globalInputCounter = 0;
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
float fEnemyStaCostWeapMulti = 2.0f;
float fEnemyStaStopThresPer = 0.3f;
float fEnemyStaLargeRecoilThresPer = 0.05f;

// Player
float fPlayerPushMulti = 0.0f;
float fPlayerStaCostMin = 6.0f;
float fPlayerStaCostMax = 50.0f;
float fPlayerStaCostWeapMulti = 2.0f;
float fPlayerWeaponSpeedRewardThres = 150.0f;
float fPlayerWeaponSpeedReward = 0.1f;
float fPlayerWeaponSpeedRewardThres2 = 30.0f;
float fPlayerWeaponSpeedReward2 = 0.5f;
int iHapticStrMin = 10;
int iHapticStrMax = 50;
float fHapticMulti = 1.0f;
int iHapticLengthMicroSec = 100000; // 100 ms

ZacOnFrame::CollisionRing ZacOnFrame::colBuffer = ZacOnFrame::CollisionRing(10);
int64_t collisionIgnoreDur = 30; 
int64_t collisionEffectDurEnemyShort = 30;  // Within 30 frames, any attack from the enemy is nullified
int64_t collisionEffectDurEnemyLong =
    90;  // Within 90 frames, only the first attack from enemy will be nullified
        //Is there any attack animation whose start and hit will be longer than 90 frames?
int64_t iDelayEnemyHit = 6;
OriMeleeQueue meleeQueue = OriMeleeQueue(20);
ZacOnFrame::SpeedRing ZacOnFrame::speedBuf = ZacOnFrame::SpeedRing(90);

namespace {
    /**
     * Setup logging.
     */
    void InitializeLogging() {
        auto path = log_directory();
        if (!path) {
            report_and_fail("Unable to lookup SKSE logs directory.");
        }
        *path /= PluginDeclaration::GetSingleton()->GetName();
        *path += L".log";

        std::shared_ptr<spdlog::logger> log;
        if (IsDebuggerPresent()) {
            log = std::make_shared<spdlog::logger>("Global", std::make_shared<spdlog::sinks::msvc_sink_mt>());
        } else {
            log = std::make_shared<spdlog::logger>(
                "Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
        }

        const auto level = spdlog::level::trace;

        log->set_level(level);
        log->flush_on(level);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] [%s:%#] %v");
    }

    /**
     * Initialize the hooks.
     */
    void InitializeHitHooks() {
        log::trace("Initializing hooks...");

        OnMeleeHit::OnMeleeHitHook::InstallHook();

        log::trace("Hooks initialized.");
    }

    void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
        log::info("MessageHandler called");
        switch (a_msg->type) {
            case SKSE::MessagingInterface::kPostPostLoad: {
                log::info("kPostPostLoad");
                const auto precisionAPI =
                    reinterpret_cast<PRECISION_API::IVPrecision4*>(PRECISION_API::RequestPluginAPI());
                if (precisionAPI) {
                    precisionAPI->AddWeaponWeaponCollisionCallback(SKSE::GetPluginHandle(), OnMeleeHit::PrecisionWeaponsCallback);
                    logger::info("Enabled compatibility with Precision");
                }
            }
                break;
            case SKSE::MessagingInterface::kDataLoaded: {
                log::info("kDataLoaded");
                auto parryingHandle = GetModuleHandleA("Parrying.dll");
                if (parryingHandle) {
                    logger::error("Warning! ParryingRPG has detected that Parrying.dll is also loaded!");
                    RE::DebugMessageBox("Warning! ParryingRPG has detected that Parrying.dll is also loaded!");
                }

                auto maxsuWeaponParryHandle = GetModuleHandleA("MaxsuWeaponParry.dll");
                if (maxsuWeaponParryHandle) {
                    logger::error("Warning! ParryingRPG has detected that MaxsuWeaponParry.dll is also loaded!");
                    RE::DebugMessageBox("Warning! ParryingRPG has detected that MaxsuWeaponParry.dll is also loaded!");
                }
            }
                break;
            
        }
    }
}  // namespace


bool BindPapyrusFunctions(RE::BSScript::IVirtualMachine* vm) {
    //papyrusVM = vm;
    return true;
}


/**
 * This is the main callback for initializing the SKSE plugin, called just before Skyrim runs its main function.
 */
SKSEPluginLoad(const LoadInterface* skse) {
    InitializeLogging();

    auto* plugin = PluginDeclaration::GetSingleton();
    auto version = plugin->GetVersion();
    log::info("{} {} is loading...", plugin->GetName(), version);

    Init(skse);

    //const auto runtimeVersion = skse->RuntimeVersion();
    //
    //if (runtimeVersion < REL::Version{1, 5, 97, 0}) {
    //    logger::error("Parrying RPG is not compatible with runtime versions below 1.5.97!");
    //    return false;
    //}

    try {
        Settings::GetSingleton()->Load();
    } catch (...) {
        logger::error("Exception caught when loading settings! Default settings will be used");
    }

    InitializeHitHooks();
    SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);

    log::info("Registered main hooks. About to register Hit and Input");


    log::info("About to hook frame update");
    ZacOnFrame::InstallFrameHook();

    log::info("About to get papyrus");
    SKSE::GetPapyrusInterface()->Register(BindPapyrusFunctions);

    log::info("{} has finished loading.", plugin->GetName());
    return true;
}
