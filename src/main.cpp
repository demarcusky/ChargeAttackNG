#include <SimpleIni.h>
#include "utils.h"


const SKSE::TaskInterface* task = nullptr;
RE::PlayerCharacter* player;

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

bool isPlayerAttacking() {
    if (player->AsActorState()->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal && !player->IsInKillMove()) {
        RE::ATTACK_STATE_ENUM currState = (player->AsActorState()->actorState1.meleeAttackState);
        if (currState >= RE::ATTACK_STATE_ENUM::kDraw && currState <= RE::ATTACK_STATE_ENUM::kBash) return true;
    }
    return false;
}

bool isDualWielding() {
    auto weaponLeft = reinterpret_cast<RE::TESObjectWEAP*>(player->GetEquippedObject(true));
    auto weaponRight = reinterpret_cast<RE::TESObjectWEAP*>(player->GetEquippedObject(false));

    return isWeaponValid(weaponLeft, true) && isWeaponValid(weaponRight, false);
}

bool isPowerAttack(float holdTime, bool isLeftHandBusy, bool isRightHandBusy, bool isBlocking) {
    if (!isDualWielding() && (isLeftHandBusy || isRightHandBusy) && !isBlocking) return false;

    auto isPowerAttack = holdTime > 0.44f; // minChargeTime, may change
    return isPowerAttack;
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
                processHold(a_event);
            }
            if (a_event->IsUp()) {
                processRelease(a_event);
            }
        } else {
            // if not doing a power attack
            if (isButtonEventValid(a_event)) {
                auto isLeft = isLeftButton(a_event);
                isLeft ? isLeftNotCharge = a_event->IsHeld() : isRightNotCharge = a_event->IsHeld();
                setIndication(indicateLeft, isLeftNotCharge);
                setIndication(indicateRight, isRightNotCharge);
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
    bool indicateLeft = false;
    bool indicateRight = false;
    bool isLeftNotCharge = false;
    bool isRightNotCharge = false;
    bool isLeftDualHeld = false;
    bool isRightDualHeld = false;

    void setIndication(bool hand, bool val) {
        hand = val;
    }

    void indicatePowerAttack(bool isLeft) {
        float holdTime = isLeft ? leftHoldTime : rightHoldTime;

        bool isBlocking = false;
        player->GetGraphVariableBool("IsBlocking", isBlocking);

        if (!isPlayerAttacking() && isPowerAttack(holdTime, isLeftNotCharge, isRightNotCharge, isBlocking)) {
            if (indicateLeft || indicateRight) return; // already charging
            setIndication(isLeft, true);
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

    void PerformAction(RE::BGSAction* action, RE::Actor* actor, int index) {
        if (task == NULL) {
            // logger::info("Tasks not initialized.");

            return;
        }
        
        task->AddTask([action, actor, index]() {
            std::unique_ptr<RE::TESActionData> data(RE::TESActionData::Create());
            data->source = RE::NiPointer<RE::TESObjectREFR>(actor);
            data->action = action;

            typedef bool func_t(RE::TESActionData*);
            REL::Relocation<func_t> func{RELOCATION_ID(40551, 41557)};
            bool succ = func(data.get());

            // if (!succ && index < ACTION_MAX_RETRY) { }
        });
    }

    void processHold(RE::ButtonEvent *button) {
        auto isLeft = isLeftButton(button);
        if (isLeft) {
            leftHoldTime = button->HeldDuration();
            isRightDualHeld = isRightDualHeld || rightHoldTime > 0.0f;
        } else {
            rightHoldTime = button->HeldDuration();
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

        timeDiff = leftTimestamp - rightTimestamp;

        bool isBlocking = false;
        player->GetGraphVariableBool("IsBlocking", isBlocking);

        if (shouldAttack || (timeDiff == 0 && isLeft)) {
            setIndication(isLeft, false);

            auto isDualHeld = isLeft ? tempIsRightDualHeld : tempIsLeftDualHeld;
            float maxHoldTime = std::max(tempLeftHoldTime, tempRightHoldTime);

            auto isAttacking = isPlayerAttacking();
            auto isPowAttack = isPowerAttack(maxHoldTime, isLeftNotCharge, isRightNotCharge, isBlocking);
            auto attackAction = getAttackAction(isLeft, timeDiff, dualWielding, isDualHeld, false);

            if (!isPowAttack || (isPowAttack && !isAttacking)) {
                PerformAction(attackAction, player, false);

                if (!isLeft && !isPowerAttack && isBlocking) {
                    PerformAction(actionRightRelease, player, false);
                }
            }

            if (isPowAttack && !isAttacking && (!isBlocking || dualWielding)) {
                attackAction = getAttackAction(isLeft, timeDiff, dualWielding, isDualHeld, true);

                PerformAction(attackAction, player, true);
            }

            if (!(attackAction == actionDualAttack || attackAction == actionDualPowerAttack)) {
                PerformAction(isLeft ? actionLeftRelease : actionRightAttack, player, false);  
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

/*
SKSE function run on startup.
*/
SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    Init(skse);

    loadSettings();

    auto task = SKSE::GetTaskInterface();
	auto message = SKSE::GetMessagingInterface();
	message->RegisterListener(onMessage);
    return true;
}