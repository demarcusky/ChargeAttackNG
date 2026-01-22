#include <SimpleIni.h>
#include "utils.h"

// ini Settings here
uint64_t leftButton = 280;
uint64_t rightButton = 281;
bool isMouseReverse = false;
// Rate

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

RE::PlayerCharacter* player;

void loadSettings() {
    constexpr auto path = L"Data/SKSE/Plugins/ChargeAttackNG.ini";

    CSimpleIniA ini;
    ini.SetUnicode();
    ini.LoadFile(path);

    leftButton = ini.GetLongValue("Input", "leftButton", 280);
    rightButton = ini.GetLongValue("Input", "rightButton", 281);
    isMouseReverse = ini.GetBoolValue("Input", "ReverseMouseButtons", false);

    // Stamina drain rate
    // damage scale rate

    (void)ini.SaveFile(path);
}

bool isWeaponValid(RE::TESObjectWEAP* weapon, bool isLeft) {
    if (weapon == NULL) return false;
    if (!weapon->IsWeapon() || weapon->IsBow() || weapon->IsCrossbow() || weapon->IsStaff()) return false;
    if (isLeft && (weapon->IsTwoHandedAxe() || weapon->IsTwoHandedSword())) return false;
    return true;
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
    if (!isButtonEventValid(a_event)) {
        return false;
    }
    
    const auto gameUI = RE::UI::GetSingleton();
    const auto controlMap = RE::ControlMap::GetSingleton();
    if (gameUI == NULL || controlMap == NULL || (gameUI && gameUI->GameIsPaused())) {
        return false;
    }
    
    if (player->IsInKillMove() || player->IsOnMount() || player->IsSneaking() || player->IsRunning() || player->IsInMidair() || player->IsInRagdollState()) {
        return false;
    }
    
    auto playerState = player->AsActorState();
    if (playerState == NULL || (playerState && (playerState->GetWeaponState() != RE::WEAPON_STATE::kDrawn ||
    playerState->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal ||
    playerState->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal ||
    playerState->GetFlyState() != RE::FLY_STATE::kNone))) {
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
                processHold(a_event);
            }

            if (a_event->IsUp()) {
                //processRelease();
            }
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

    float leftHoldTime = 0.0f;
    float rightHoldTime = 0.0f;
    bool leftAltBehavior = false;
    bool rightAltBehavior = false;
    bool isLeftDualHeld = false;
    bool isRightDualHeld = false;

    void TryIndicatePowerAttack(bool isLeft) {
        float holdTime = isLeft ? leftHoldTime : rightHoldTime;

        /*bool isBlocking = false;
        player->GetGraphVariableBool("IsBlocking", isBlocking);

        bool isPlayerAttacking = IsPlayerAttacking(player);
        bool isPowerAttack = IsPowerAttackAlt(player, holdTime, leftAltBehavior, rightAltBehavior, isBlocking);
        
        if (!isPlayerAttacking && isPowerAttack) {
            if (isLeftAttackIndicated || isRightAttackIndicated) {
                return;
            }

            SetIsAttackIndicated(isLeft, true);
        } else {
            SetIsAttackIndicated(isLeft, false);
        }*/
        
    }
    
    void processHold(RE::ButtonEvent *button) {
        auto isLeft = isLeftButton(button);
        if (isLeft) {
            leftHoldTime = button->HeldDuration();
            leftAltBehavior = false;
            isRightDualHeld = isRightDualHeld || rightHoldTime > 0.0f;
        } else {
            rightHoldTime = button->HeldDuration();
            rightAltBehavior = false;
            isLeftDualHeld = isLeftDualHeld || leftHoldTime > 0.0f;
        }
        TryIndicatePowerAttack(isLeft);
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

/*
SKSE function run on startup.
*/
SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    Init(skse);

    loadSettings();

    auto g_task = SKSE::GetTaskInterface();
	auto g_message = SKSE::GetMessagingInterface();
	g_message->RegisterListener(onMessage);
    return true;
}