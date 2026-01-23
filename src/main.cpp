#include <SimpleIni.h>
#include "utils.h"

const SKSE::TaskInterface* tasks = nullptr;
RE::PlayerCharacter* player;

// INI Settings
uint64_t leftButton = 280;
uint64_t rightButton = 281;
bool isMouseReverse = false;
// Rate

// Action IDs
RE::BGSAction* actionRightAttack;
RE::BGSAction* actionLeftAttack;
RE::BGSAction* actionDualAttack;
RE::BGSAction* actionRightPowerAttack;
RE::BGSAction* actionLeftPowerAttack;
RE::BGSAction* actionDualPowerAttack;
RE::BGSAction* actionLeftRelease;
RE::BGSAction* actionRightRelease;

RE::BSAudioManager* audioManager = NULL;

RE::BGSSoundDescriptorForm* powerAttackSound;

// Global Variables
float leftHoldTime = 0.0f;
float rightHoldTime = 0.0f;
bool indicateLeft = false;
bool indicateRight = false;
bool isLeftNotCharge = false;
bool isRightNotCharge = false;
bool isLeftDualHeld = false;
bool isRightDualHeld = false;

void loadSettings() {
    constexpr auto path = L"Data/SKSE/Plugins/ChargeAttackNG.ini";

    CSimpleIniA ini;
    ini.SetUnicode();
    ini.LoadFile(path);

    leftButton = ini.GetLongValue("Input", "leftButton", 280);
    rightButton = ini.GetLongValue("Input", "rightButton", 281);
    isMouseReverse = ini.GetBoolValue("Input", "reverseMouseButtons", false);
    // Stamina drain rate
    // damage scale rate

    ini.SetLongValue("Input", "leftButton", leftButton);
    ini.SetLongValue("Input", "rightButton", rightButton);
    ini.SetLongValue("Input", "reverseMouseButtons", isMouseReverse);

    (void)ini.SaveFile(path);
}

bool isPlayerAttacking() {
    if (player->AsActorState()->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal && !player->IsInKillMove()) {
        RE::ATTACK_STATE_ENUM currState = (player->AsActorState()->actorState1.meleeAttackState);
        if (currState >= RE::ATTACK_STATE_ENUM::kDraw && currState <= RE::ATTACK_STATE_ENUM::kBash) return true;
    }
    return false;
}

bool isWeaponValid(RE::TESObjectWEAP* weapon, bool isLeft) {
    if (weapon == NULL) return false;
    if (!weapon->IsWeapon() || weapon->IsBow() || weapon->IsCrossbow() || weapon->IsStaff()) return false;
    if (isLeft && (weapon->IsTwoHandedAxe() || weapon->IsTwoHandedSword())) return false;
    return true;
}

bool isDualWielding() {
    auto weaponLeft = reinterpret_cast<RE::TESObjectWEAP*>(player->GetEquippedObject(true));
    auto weaponRight = reinterpret_cast<RE::TESObjectWEAP*>(player->GetEquippedObject(false));

    return isWeaponValid(weaponLeft, true) && isWeaponValid(weaponRight, false);
}

bool isPowerAttack(float holdTime, bool isLeftHandBusy, bool isRightHandBusy, bool isBlocking) {
    if (!isDualWielding() && (isLeftHandBusy || isRightHandBusy) && !isBlocking) {
        return false;
    }
    auto isPowerAttack = holdTime > 0.44f; // minChargeTime, may change
    return isPowerAttack;
}

bool isLeftButton(RE::ButtonEvent* a_event) {
    auto device = a_event->device.get();
    auto keyMask = a_event->GetIDCode();

    if (device == RE::INPUT_DEVICE::kMouse && keyMask == (uint32_t)(isMouseReverse ? 0 : 1)) return true;
    if (device == RE::INPUT_DEVICE::kGamepad && getKeycode(keyMask) == leftButton) return true;

    return false;
}

bool isButtonEventValid(RE::ButtonEvent* a_event) {
    auto device = a_event->device.get();
    auto keyMask = a_event->GetIDCode();

    if ((device != RE::INPUT_DEVICE::kMouse && device != RE::INPUT_DEVICE::kGamepad) ||
        (device == RE::INPUT_DEVICE::kGamepad && getKeycode(keyMask) != leftButton && getKeycode(keyMask) != rightButton) ||
        (device == RE::INPUT_DEVICE::kMouse && keyMask != 0 && keyMask != 1))
    {
        return false;
    }

    return true;
}

bool isEventValid(RE::ButtonEvent* a_event) {
    if (!isButtonEventValid(a_event)) return false;
    
    const auto gameUI = RE::UI::GetSingleton();
    const auto controlMap = RE::ControlMap::GetSingleton();
    if (gameUI == NULL || controlMap == NULL || (gameUI && gameUI->GameIsPaused())) return false;
    
    if (player->IsInKillMove() || player->IsOnMount() || player->IsSneaking() || player->IsRunning() || player->IsInMidair() || player->IsInRagdollState()) {
        return false;
    }

    auto playerState = player->AsActorState();
    if (playerState == NULL || (playerState && (playerState->GetWeaponState() != RE::WEAPON_STATE::kDrawn ||
        playerState->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal ||
        playerState->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal ||
        playerState->GetFlyState() != RE::FLY_STATE::kNone)))
    {
        return false;
    }

    auto isLeft = isLeftButton(a_event);
    auto weaponLeft = reinterpret_cast<RE::TESObjectWEAP*>(player->GetEquippedObject(true));
    auto weaponRight = reinterpret_cast<RE::TESObjectWEAP*>(player->GetEquippedObject(false));
    auto weapon = isLeft ? weaponLeft : weaponRight;

    return isWeaponValid(weapon, isLeft);
}

/*
This class handles the trampoline hooks in the AttackBlockHandler.
*/
class HookAttackBlockHandler {
public:
	typedef void (HookAttackBlockHandler::* FnProcessButton) (RE::ButtonEvent*, void*);

	void ProcessButton(RE::ButtonEvent* a_event, void* a_data) {
        if (isEventValid(a_event)) {
            if (a_event->IsDown() || a_event->IsHeld()) {
                SKSE::log::info("Button press/hold...");
                processHold(a_event);
            }
            if (a_event->IsUp()) {
                SKSE::log::info("Button release...");
                processRelease(a_event);
            }
            return;
        }

        // if not doing a power attack
        if (isButtonEventValid(a_event)) {
            auto isLeft = isLeftButton(a_event);
            bool alternate;
            if (isLeft) {
                isLeftNotCharge = a_event->IsHeld();
                alternate = isLeftNotCharge;
            } else {
                isRightNotCharge = a_event->IsHeld();
                alternate = isRightNotCharge;
            }

            if (alternate) setIndication(isLeft, false);
        }

		FnProcessButton fn = fnHash.at(*(uintptr_t*)this);
		if (fn) (this->*fn)(a_event, a_data);
	}

    // Function that initiates hooking.
    // Saves the original function in the fnHash map.
	static void Hook() {
		REL::Relocation<uintptr_t> vtable{ RE::VTABLE_AttackBlockHandler[0] };
		FnProcessButton fn = SKSE::stl::unrestricted_cast<FnProcessButton>(vtable.write_vfunc(4, &HookAttackBlockHandler::ProcessButton));
		fnHash.insert(std::pair<uintptr_t, FnProcessButton>(vtable.address(), fn));
	}
private:
	static std::unordered_map<uintptr_t, FnProcessButton> fnHash;

    void setIndication(bool isLeft, bool val) {
        if (isLeft) {
            indicateLeft = val;
        } else {
            indicateRight = val;
        }
    }

    void indicatePowerAttack(bool isLeft) {
        float holdTime = isLeft ? leftHoldTime : rightHoldTime;

        bool isBlocking = false;
        player->GetGraphVariableBool("IsBlocking", isBlocking);

        if (!isPlayerAttacking() && isPowerAttack(holdTime, isLeftNotCharge, isRightNotCharge, isBlocking)) {
            if (indicateLeft || indicateRight) return; // already charging
            setIndication(isLeft, true);
            SKSE::log::info("Initiating charge attack...");
        } else {
            setIndication(isLeft, false);
        }
    }

    RE::BGSAction* getAttackAction(bool isLeft, uint64_t timeDiff, bool isDualWielding, bool isDualHeld, bool isPowerAttack) {
        if (isDualWielding && isDualHeld && timeDiff < 130) {
            return isPowerAttack ? actionDualPowerAttack : actionDualAttack;
        }

        if (isLeft) {
            return isPowerAttack ? actionLeftPowerAttack : actionLeftAttack;
        }

        return isPowerAttack ? actionRightPowerAttack : actionRightAttack;
    }

    void performAction(RE::BGSAction* action, RE::Actor* actor) {
        if (tasks == NULL) {
            SKSE::log::info("ERROR: Tasks not initialised...");
            return;
        }

        tasks->AddTask([action, actor]() {
            std::unique_ptr<RE::TESActionData> data(RE::TESActionData::Create());
            data->source = RE::NiPointer<RE::TESObjectREFR>(actor);
            data->action = action;

            typedef bool func_t(RE::TESActionData*);
            REL::Relocation<func_t> func{RELOCATION_ID(40551, 41557)};

            bool success = func(data.get());
            if (!success) {
                SKSE::log::info("Action failed: {}", (void*)action);
            }
        });
    }

    void processHold(RE::ButtonEvent *button) {
        auto isLeft = isLeftButton(button);
        if (isLeft) {
            leftHoldTime = button->HeldDuration();
            isLeftNotCharge = false;
            isRightDualHeld = isRightDualHeld || rightHoldTime > 0.0f;
        } else {
            rightHoldTime = button->HeldDuration();
            isRightNotCharge = false;
            isLeftDualHeld = isLeftDualHeld || leftHoldTime > 0.0f;
        }
        indicatePowerAttack(isLeft);
    }

    void processRelease(RE::ButtonEvent *button) {
        auto isLeft = isLeftButton(button);

        auto tempLeftHoldTime = leftHoldTime;
        auto tempRightHoldTime = rightHoldTime;

        auto tempIsLeftDualHeld = isLeftDualHeld;
        auto tempIsRightDualHeld = isRightDualHeld;

        uint64_t leftTimestamp = 0;
        uint64_t rightTimestamp = 0;

        auto dualWielding = isDualWielding();

        auto shouldAttack = false;
        uint64_t timeDiff = 0;

        if (isLeft) {
            leftHoldTime = 0.0f;
            leftTimestamp = timestamp();
            isRightDualHeld = false;
            shouldAttack = tempRightHoldTime == 0.0f;
        } else {
            rightHoldTime = 0.0f;
            rightTimestamp = timestamp();
            isLeftDualHeld = false;
            shouldAttack = tempLeftHoldTime == 0.0f;
        }

        timeDiff = _abs64(leftTimestamp - rightTimestamp);

        bool isBlocking = false;
        player->GetGraphVariableBool("IsBlocking", isBlocking);

        if (shouldAttack || (timeDiff == 0 && isLeft)) {
            SKSE::log::info("Starting attack action...");
            setIndication(isLeft, false);

            auto isDualHeld = isLeft ? tempIsRightDualHeld : tempIsLeftDualHeld;
            float maxHoldTime = std::max(tempLeftHoldTime, tempRightHoldTime);

            auto isAttacking = isPlayerAttacking();
            auto isPowAttack = isPowerAttack(maxHoldTime, isLeftNotCharge, isRightNotCharge, isBlocking);
            auto attackAction = getAttackAction(isLeft, timeDiff, dualWielding, isDualHeld, false);

            if (!isPowAttack || (isPowAttack && !isAttacking)) {
                performAction(attackAction, player);
                if (!isLeft && !isPowAttack && isBlocking) {
                    performAction(actionRightRelease, player);
                }
            }

            if (isPowAttack && !isAttacking && (!isBlocking || dualWielding)) {
                attackAction = getAttackAction(isLeft, timeDiff, dualWielding, isDualHeld, true);
                performAction(attackAction, player);
            }

            if (!(attackAction == actionDualAttack || attackAction == actionDualPowerAttack)) {
                performAction(isLeft ? actionLeftRelease : actionRightAttack, player);
            }
        }
    }
};
std::unordered_map<uintptr_t, HookAttackBlockHandler::FnProcessButton> HookAttackBlockHandler::fnHash;

/*
This function enacts logic based on message type.
Main functionality is called after all initial data is loaded.
*/
void onMessage(SKSE::MessagingInterface::Message* msg) {
    if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
        player = RE::PlayerCharacter::GetSingleton();

        actionRightAttack = (RE::BGSAction*)RE::TESForm::LookupByID(0x13005);
        actionLeftAttack = (RE::BGSAction*)RE::TESForm::LookupByID(0x13004);
        actionDualAttack = (RE::BGSAction*)RE::TESForm::LookupByID(0x50c96);
        actionRightPowerAttack = (RE::BGSAction*)RE::TESForm::LookupByID(0x13383);
        actionLeftPowerAttack = (RE::BGSAction*)RE::TESForm::LookupByID(0x2e2f6);
        actionDualPowerAttack = (RE::BGSAction*)RE::TESForm::LookupByID(0x2e2f7);
        actionLeftRelease = (RE::BGSAction*)RE::TESForm::LookupByID(0x13451);
        actionRightRelease = (RE::BGSAction*)RE::TESForm::LookupByID(0x13454);
        powerAttackSound = (RE::BGSSoundDescriptorForm*)RE::TESForm::LookupByID(0x10eb7a);

        audioManager = RE::BSAudioManager::GetSingleton();

        HookAttackBlockHandler::Hook();

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
SKSE function run on startup.
*/
SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    initLog();
    SKSE::log::info("Game version : {}", skse->RuntimeVersion().string());

    SKSE::Init(skse);
    
    loadSettings();
    SKSE::log::info("Settings loaded...");

    tasks = SKSE::GetTaskInterface();
	SKSE::GetMessagingInterface()->RegisterListener(onMessage);

    return true;
}