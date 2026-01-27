#include "attackBlockHandler.h"
#include "utils.h"

RE::BSAudioManager* audioManager = NULL;
RE::BGSSoundDescriptorForm* powerAttackSound;

void initLog() {
    auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder) SKSE::stl::report_and_fail("SKSE log_directory not provided.");

    auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
    auto logFilePath = *logsFolder / std::format("{}.log", pluginName);
    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));

    spdlog::set_default_logger(std::move(loggerPtr));
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::trace);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] [%s:%#] %v");
}

/*
This function enacts logic based on message type.
Main functionality is called after all initial data is loaded.
*/
void onMessage(SKSE::MessagingInterface::Message* msg) {
    if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
        powerAttackSound = (RE::BGSSoundDescriptorForm*)RE::TESForm::LookupByID(0x10eb7a);

        audioManager = RE::BSAudioManager::GetSingleton();

        HookAttackBlockHandler::initialise();

        /*if (current ue = ue->attackPowerStart) {
                if (p.iscrouching || p.isSprinting || p.isRidingHorse) {
                    trigger vanilla actions
                }
 
                while player is holding power attack button {
                    slow movement
                    drain stamina at a fixed rate (possibly customisable with MCM?)
                    scale power attack damage at a fixed rate (possibly customisable with MCM?)
                    vibration when charging??
                }

                play attack animation
                settings to adjust when the charge occurs during animation? should happen in v2
            }*/ 
    }
}

/*
SKSE function run on startup.
*/
SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    initLog();
    SKSE::log::info("Game version : {}", skse->RuntimeVersion().string());

    SKSE::Init(skse);
    SKSE::AllocTrampoline(1 << 10);
    
    HookAttackBlockHandler::loadSettings();
    SKSE::log::info("Settings loaded...");

    HookAttackBlockHandler::tasks = SKSE::GetTaskInterface();
	SKSE::GetMessagingInterface()->RegisterListener(onMessage);

    return true;
}