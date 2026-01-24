#include <SimpleIni.h>
#include "attackBlockHandler.h"

float leftHoldTime = 0.0f;
float rightHoldTime = 0.0f;
bool indicateLeft = false;
bool indicateRight = false;
bool isLeftNotCharge = false;
bool isRightNotCharge = false;
bool isLeftDualHeld = false;
bool isRightDualHeld = false;

float currSpeed = 0.0f;

template <typename T>
std::unordered_map<uintptr_t, T>& GetFnHash() {
    static std::unordered_map<uintptr_t, T> fnHash;
    return fnHash;
}

/*

HELPER FUNCTIONS

*/

bool isPlayerAttacking(RE::PlayerCharacter* player) {
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

bool isDualWielding(RE::PlayerCharacter* player) {
    auto weaponLeft = reinterpret_cast<RE::TESObjectWEAP*>(player->GetEquippedObject(true));
    auto weaponRight = reinterpret_cast<RE::TESObjectWEAP*>(player->GetEquippedObject(false));

    return isWeaponValid(weaponLeft, true) && isWeaponValid(weaponRight, false);
}

bool isPowerAttack(RE::PlayerCharacter* player, float holdTime, bool isLeftHandBusy, bool isRightHandBusy, bool isBlocking) {
    if (!isDualWielding(player) && (isLeftHandBusy || isRightHandBusy) && !isBlocking) return false;
    auto isPowerAttack = holdTime > 0.44f; // minChargeTime, may change
    return isPowerAttack;
}

bool isLeftButton(const RE::ButtonEvent* a_event) {
    auto device = a_event->device.get();
    auto keyMask = a_event->GetIDCode();

    if (device == RE::INPUT_DEVICE::kMouse && keyMask == (uint32_t)(HookAttackBlockHandler::Settings::isMouseReverse ? 0 : 1)) return true;
    if (device == RE::INPUT_DEVICE::kGamepad && getKeycode(keyMask) == HookAttackBlockHandler::Settings::leftButton) return true;

    return false;
}

bool isButtonEventValid(const RE::ButtonEvent* a_event) {
    auto device = a_event->device.get();
    auto keyMask = a_event->GetIDCode();

    if ((device != RE::INPUT_DEVICE::kMouse && device != RE::INPUT_DEVICE::kGamepad) ||
        (device == RE::INPUT_DEVICE::kGamepad && getKeycode(keyMask) != HookAttackBlockHandler::Settings::leftButton && getKeycode(keyMask) != HookAttackBlockHandler::Settings::rightButton) ||
        (device == RE::INPUT_DEVICE::kMouse && keyMask != 0 && keyMask != 1))
    {
        return false;
    }

    return true;
}

bool isEventValid(RE::PlayerCharacter* player, const RE::ButtonEvent* a_event) {
    if (!isButtonEventValid(a_event)) return false;
    
    const auto gameUI = RE::UI::GetSingleton();
    const auto controlMap = RE::ControlMap::GetSingleton();
    if (gameUI == NULL || controlMap == NULL || (gameUI && gameUI->GameIsPaused())) return false;
    
    bool isBlocking = false;
    player->GetGraphVariableBool("IsBlocking", isBlocking);
    if (player->IsInKillMove() || player->IsOnMount() || player->IsInMidair() || player->IsInRagdollState() || isPlayerAttacking(player) || isBlocking) {
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
    auto weapon = isLeft ? reinterpret_cast<RE::TESObjectWEAP*>(player->GetEquippedObject(true)) : reinterpret_cast<RE::TESObjectWEAP*>(player->GetEquippedObject(false));
    return isWeaponValid(weapon, isLeft);
}

/*

HANDLER FUNCTIONS

*/
void HookAttackBlockHandler::beginCharge(const RE::ButtonEvent *button) {
    auto holdTime = button->HeldDuration();

    if (holdTime < 0.4f) return;

    float slowdownProgress = std::min((holdTime - 0.6f) / 4.4f, 1.0f); 
    float t = 1.0f - std::pow(1.0f - slowdownProgress, 3.0f);
    float newSpeed = 1.0f - (0.95f * t);

    player->AsActorValueOwner()->SetActorValue(RE::ActorValue::kSpeedMult, newSpeed);

    // force state change to refresh speed
    const float before = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight);
    player->AsActorValueOwner()->ModActorValue(RE::ActorValue::kCarryWeight, +0.001f);
    player->AsActorValueOwner()->ModActorValue(RE::ActorValue::kCarryWeight, -0.001f);
    const float after = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight);
    const float diff = after - before;
    if (std::fabs(diff) > 1e-6f) {
        player->AsActorValueOwner()->ModActorValue(RE::ActorValue::kCarryWeight, -diff);
    }
}

void HookAttackBlockHandler::UpdateHeldStateActive(const RE::ButtonEvent* a_event) {
    if (isEventValid(player, a_event)) {
        SKSE::log::info("Holding button...");
        beginCharge(a_event);
        return;
    }
    
    FnUpdateHeldStateActive fn = GetFnHash<FnUpdateHeldStateActive>().at(*(uintptr_t*)this);
    if (fn) (this->*fn)(a_event);
}

void HookAttackBlockHandler::Hook() {
    REL::Relocation<uintptr_t> vtable{ RE::VTABLE_AttackBlockHandler[0] };
    FnUpdateHeldStateActive fnUpdateHeldStateActive = SKSE::stl::unrestricted_cast<FnUpdateHeldStateActive>(vtable.write_vfunc(5, &HookAttackBlockHandler::UpdateHeldStateActive));
    GetFnHash<FnUpdateHeldStateActive>().insert(std::pair<uintptr_t, FnUpdateHeldStateActive>(vtable.address(), fnUpdateHeldStateActive));
}

void HookAttackBlockHandler::initialise() {
    player = RE::PlayerCharacter::GetSingleton();

    actionRightAttack = (RE::BGSAction*)RE::TESForm::LookupByID(0x13005);
    actionLeftAttack = (RE::BGSAction*)RE::TESForm::LookupByID(0x13004);
    actionDualAttack = (RE::BGSAction*)RE::TESForm::LookupByID(0x50c96);
    actionRightPowerAttack = (RE::BGSAction*)RE::TESForm::LookupByID(0x13383);
    actionLeftPowerAttack = (RE::BGSAction*)RE::TESForm::LookupByID(0x2e2f6);
    actionDualPowerAttack = (RE::BGSAction*)RE::TESForm::LookupByID(0x2e2f7);
    actionLeftRelease = (RE::BGSAction*)RE::TESForm::LookupByID(0x13451);
    actionRightRelease = (RE::BGSAction*)RE::TESForm::LookupByID(0x13454);

    SKSE::log::info("Data initialised...");

    Hook();
}

void HookAttackBlockHandler::loadSettings() {
    constexpr auto path = L"Data/SKSE/Plugins/ChargeAttackNG.ini";

    CSimpleIniA ini;
    ini.SetUnicode();
    ini.LoadFile(path);

    HookAttackBlockHandler::Settings::leftButton = ini.GetLongValue("Input", "leftButton", 280);
    HookAttackBlockHandler::Settings::rightButton = ini.GetLongValue("Input", "rightButton", 281);
    HookAttackBlockHandler::Settings::isMouseReverse = ini.GetBoolValue("Input", "reverseMouseButtons", false);
    // Stamina drain rate
    // damage scale rate

    ini.SetLongValue("Input", "leftButton", HookAttackBlockHandler::Settings::leftButton);
    ini.SetLongValue("Input", "rightButton", HookAttackBlockHandler::Settings::rightButton);
    ini.SetLongValue("Input", "reverseMouseButtons", HookAttackBlockHandler::Settings::isMouseReverse);

    (void)ini.SaveFile(path);
}
