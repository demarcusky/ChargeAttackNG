#include "chargeAttackHandler.h"
#include "utils.h"

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
        ChargeAttackHandler::initialise();
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

	SKSE::GetMessagingInterface()->RegisterListener(onMessage);

    return true;
}