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

uint64_t leftRelease = 0;
uint64_t rightRelease = 0;

bool bCharging = false;
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

RE::EffectSetting* CreateSlowEffect() {
    static RE::EffectSetting* slowEffect = nullptr;
    if (slowEffect) return slowEffect;

    auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::EffectSetting>();
    if (!factory) return nullptr;

    slowEffect = factory->Create();
    if (!slowEffect) {
        SKSE::log::error("Failed to create slow effect");
        return nullptr;
    }

    slowEffect->data.flags.set(
        RE::EffectSetting::EffectSettingData::Flag::kRecover,
        RE::EffectSetting::EffectSettingData::Flag::kDetrimental,
        RE::EffectSetting::EffectSettingData::Flag::kNoHitEvent,
        RE::EffectSetting::EffectSettingData::Flag::kNoDuration,
        RE::EffectSetting::EffectSettingData::Flag::kNoArea,
        RE::EffectSetting::EffectSettingData::Flag::kNoRecast,
        RE::EffectSetting::EffectSettingData::Flag::kPainless,
        RE::EffectSetting::EffectSettingData::Flag::kNoHitEffect,
        RE::EffectSetting::EffectSettingData::Flag::kNoDeathDispel
    );
    slowEffect->data.baseCost = 0.0f;
    slowEffect->data.associatedForm = RE::TESForm::LookupByID(0xb729e);
    slowEffect->data.associatedSkill = RE::ActorValue::kNone;
    slowEffect->data.resistVariable = RE::ActorValue::kNone;
    slowEffect->data.numCounterEffects = 0;
    slowEffect->data.castingArt = nullptr;
    slowEffect->data.taperWeight = 0.0f;
    slowEffect->data.enchantShader = nullptr;
    slowEffect->data.minimumSkill = 0;
    slowEffect->data.spellmakingArea = 0;
    slowEffect->data.spellmakingChargeTime = 0.0f;
    slowEffect->data.taperCurve = 0.0f;
    slowEffect->data.taperDuration = 0.0f;
    slowEffect->data.secondAVWeight = 0.0f;
    
    slowEffect->data.archetype = RE::EffectArchetypes::ArchetypeID::kPeakValueModifier;
    slowEffect->data.primaryAV = RE::ActorValue::kSpeedMult;
    slowEffect->data.projectileBase = nullptr;
    slowEffect->data.explosion = nullptr;
    slowEffect->data.castingType = RE::MagicSystem::CastingType::kConstantEffect;
    slowEffect->data.delivery = RE::MagicSystem::Delivery::kSelf;
    slowEffect->data.secondaryAV = RE::ActorValue::kNone;
    slowEffect->data.castingArt = nullptr;
    slowEffect->data.hitEffectArt = nullptr;
    slowEffect->data.impactDataSet = nullptr;
    slowEffect->data.skillUsageMult = 0.0f;
    slowEffect->data.dualCastData = nullptr;
    slowEffect->data.dualCastScale = 1.0f;
    slowEffect->data.enchantEffectArt = nullptr;
    slowEffect->data.hitVisuals = nullptr;
    slowEffect->data.enchantVisuals = nullptr;
    slowEffect->data.equipAbility = nullptr;
    slowEffect->data.imageSpaceMod = nullptr;
    slowEffect->data.perk = nullptr;
    slowEffect->data.castingSoundLevel = RE::SOUND_LEVEL::kSilent;
    slowEffect->data.aiScore = 0.0f;
    slowEffect->data.aiDelayTimer = 0.0f;

    return slowEffect;
}

RE::SpellItem* CreateSlowSpell() {
    static RE::SpellItem* slowSpell = nullptr;

    if (slowSpell) return slowSpell;

    auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::SpellItem>();
    if (!factory) return nullptr;
    
    slowSpell = factory->Create();
    if (!slowSpell) return nullptr;
    slowSpell->data.costOverride = 0;
    slowSpell->data.flags.set(RE::SpellItem::SpellFlag::kNoAbsorb, RE::SpellItem::SpellFlag::kIgnoreResistance);
    slowSpell->data.spellType = RE::MagicSystem::SpellType::kAbility;
    slowSpell->data.chargeTime = 0.0;
    slowSpell->data.castingType = RE::MagicSystem::CastingType::kConstantEffect;
    slowSpell->data.delivery = RE::MagicSystem::Delivery::kSelf;
    slowSpell->data.castDuration = 0.0f;
    slowSpell->data.range = 0.0f;
    slowSpell->data.castingPerk = nullptr;

    auto slowEffect = CreateSlowEffect();
    if (slowEffect) {
        auto newEffect = new RE::Effect();
        newEffect->baseEffect = slowEffect;
        newEffect->effectItem.magnitude = 50.0f;  // 50% slow
        newEffect->effectItem.area = 0;
        newEffect->effectItem.duration = 99999;  // 5 seconds
        newEffect->cost = 0.0f;
        
        slowSpell->effects.push_back(newEffect);
    }
    
    return slowSpell;
}

/*

HANDLER FUNCTIONS

*/

bool HookAttackBlockHandler::isAttackEvent(const RE::ButtonEvent* a_event) {
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

void HookAttackBlockHandler::setIndication(bool isLeft, bool val) {
    if (isLeft) {
        indicateLeft = val;
    } else {
        indicateRight = val;
    }
}

RE::BGSAction* HookAttackBlockHandler::getAttackAction(bool isLeft, bool isDualAttack, float holdTime) {
    bool isPowerAttack = holdTime >= Settings::minHoldTime;
    if (isDualAttack) return isPowerAttack ? actionDualPowerAttack : actionDualAttack;
    if (isLeft) return isPowerAttack  ? actionLeftPowerAttack : actionLeftAttack;
    return isPowerAttack ? actionRightPowerAttack : actionRightAttack;
}

void HookAttackBlockHandler::charge(float holdTime) {
    if (holdTime < Settings::minHoldTime) return;
    
    if (!bCharging) {
        bCharging = true;
        SKSE::log::info("Charging...");

        auto slowSpell = CreateSlowSpell();
        auto caster = player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
        caster->CastSpellImmediate(
            slowSpell,
            false,
            player,
            1.0f,
            false,
            0.0f,
            nullptr
        );
    }
}

void HookAttackBlockHandler::performAction(RE::BGSAction* action, RE::Actor* actor) {
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

void HookAttackBlockHandler::processHold(RE::ButtonEvent *button) {
    auto isLeft = isLeftButton(button);
    if (isLeft) {
        leftHoldTime = button->HeldDuration();
    } else {
        rightHoldTime = button->HeldDuration();
    }

    float holdTime = isLeft ? leftHoldTime : rightHoldTime;
    if (!bCharging) {
        setIndication(isLeft, true);
        SKSE::log::info("Initiating charge attack...");
    }
    charge(holdTime);

}

void HookAttackBlockHandler::processRelease(RE::ButtonEvent *button) {
    auto isLeft = isLeftButton(button);

    auto holdDuration = isLeft ? leftHoldTime : rightHoldTime;
    if (isLeft) {
        leftHoldTime = 0;
        leftRelease = timestamp();
    } else {
        rightHoldTime = 0;
        rightRelease = timestamp();
    }

    uint64_t timeDiff = _abs64(leftRelease - rightRelease);

    constexpr uint64_t DUAL_ATTACK_WINDOW = 130; // 130ms
    bool isDualAttack = (timeDiff <= DUAL_ATTACK_WINDOW) && isDualWielding(player) && (leftRelease > 0 && rightRelease > 0);
    
    if (!isDualAttack || (isDualAttack && !isLeft)) {
        SKSE::log::info("Starting attack action... (dual: {})", isDualAttack);
        
        auto attackAction = getAttackAction(isLeft, isDualAttack, holdDuration);
        performAction(attackAction, player);
        
        if (isDualAttack) {
            leftRelease = 0;
            rightRelease = 0;
        }
    }

    // Cleanup
    setIndication(isLeft, false);
    if (bCharging) {
        bCharging = false;

        RE::BSPointerHandle<RE::Actor> nullHandle;
        player->GetMagicTarget()->DispelEffect(CreateSlowSpell(), nullHandle);
    }
}

void HookAttackBlockHandler::ProcessButton(RE::ButtonEvent* a_event, void* a_data) {
    if (isAttackEvent(a_event)) {
        if (a_event->IsHeld()) {
            SKSE::log::info("Button held...");
            processHold(a_event);
        } else if (a_event->IsUp()) {
            SKSE::log::info("Button released...");
            processRelease(a_event);
        }
        return;
    }

    FnProcessButton fn = GetFnHash<FnProcessButton>().at(*(uintptr_t*)this);
    if (fn) (this->*fn)(a_event, a_data);
}

void HookAttackBlockHandler::Hook() {
    REL::Relocation<uintptr_t> vtable{ RE::VTABLE_AttackBlockHandler[0] };
    FnProcessButton fn = SKSE::stl::unrestricted_cast<FnProcessButton>(vtable.write_vfunc(4, &HookAttackBlockHandler::ProcessButton));
    GetFnHash<FnProcessButton>().insert(std::pair<uintptr_t, FnProcessButton>(vtable.address(), fn));
}

void HookAttackBlockHandler::initialise() {
    player = RE::PlayerCharacter::GetSingleton();

    actionRightAttack = (RE::BGSAction*)RE::TESForm::LookupByID(0x13005);
    actionLeftAttack = (RE::BGSAction*)RE::TESForm::LookupByID(0x13004);
    actionDualAttack = (RE::BGSAction*)RE::TESForm::LookupByID(0x50c96);
    actionRightPowerAttack = (RE::BGSAction*)RE::TESForm::LookupByID(0x13383);
    actionLeftPowerAttack = (RE::BGSAction*)RE::TESForm::LookupByID(0x2e2f6);
    actionDualPowerAttack = (RE::BGSAction*)RE::TESForm::LookupByID(0x2e2f7);

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
    HookAttackBlockHandler::Settings::minHoldTime = ini.GetDoubleValue("Input", "minimumHoldTime", 0.44f);
    // Stamina drain rate
    // damage scale rate

    ini.SetLongValue("Input", "leftButton", HookAttackBlockHandler::Settings::leftButton);
    ini.SetLongValue("Input", "rightButton", HookAttackBlockHandler::Settings::rightButton);
    ini.SetBoolValue("Input", "reverseMouseButtons", HookAttackBlockHandler::Settings::isMouseReverse);
    ini.SetDoubleValue("Gameplay", "minimumHoldTime", HookAttackBlockHandler::Settings::minHoldTime);

    (void)ini.SaveFile(path);
}
