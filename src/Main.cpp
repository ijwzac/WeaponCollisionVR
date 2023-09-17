/*
For those who are looking at the source code, feel free to use any code or assets in this project.
In fact, the code you see here is based on many other projects and I received help from many modders.
Including:
https://github.com/DennisSoemers/ParryingRPG My project starts as a patch to this mod, and ends up with a complete overhaul. I used lots of code and the main frame of this mod

*/


#include <stddef.h>
#include "OnMeleeHit.h"
#include "PrecisionAPI.h"
#include "Settings.h"
#include "Utils.h"
#include "OnFrame.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

ZacOnFrame::CollisionRing ZacOnFrame::colBuffer = ZacOnFrame::CollisionRing(10);
ZacOnFrame::SpeedRing ZacOnFrame::speedBuf = ZacOnFrame::SpeedRing(90);
OriMeleeQueue meleeQueue = OriMeleeQueue(20);

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

        const auto level = spdlog::level::info;

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
                // TODO: change name of this dll and detect parrying rpg dll
                auto handToHandHandle = GetModuleHandleA("HandToHand.dll");
                if (handToHandHandle) {
                    logger::trace("Hand to hand loaded");
                    bHandToHandLoad = true;
                }
            }
                break;
            
        }
    }
}  // namespace



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

    spdlog::level::level_enum level = spdlog::level::info;
    if (bEnableTrace) level = spdlog::level::trace;
    spdlog::default_logger()->set_level(level);

    if (bEnableWholeMod == false) {
        log::info("{} is disabled.", plugin->GetName());
        return true;
    }

    log::info("Init data struct");

    InitializeHitHooks();
    SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);

    log::info("Registered main hooks. About to register Menu Open");
    auto& eventProcessor = EventProcessor::GetSingleton();
    RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&eventProcessor);

    log::info("About to hook frame update");
    ZacOnFrame::InstallFrameHook();


    log::info("{} has finished loading.", plugin->GetName());
    return true;
}
