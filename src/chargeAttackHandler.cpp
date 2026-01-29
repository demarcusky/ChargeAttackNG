#include <SimpleIni.h>
#include "chargeAttackHandler.h"

float leftHoldTime = 0.0f;
float rightHoldTime = 0.0f;
uint64_t leftRelease = 0;
uint64_t rightRelease = 0;

bool bCharging = false;
float kDamageMult = 1.0;

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
    if (!weapon) return false;
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
    if (device == RE::INPUT_DEVICE::kMouse && keyMask == (uint32_t)(ChargeAttackHandler::Settings::isMouseReverse ? 0 : 1)) return true;
    if (device == RE::INPUT_DEVICE::kGamepad && getKeycode(keyMask) == ChargeAttackHandler::Settings::leftButton) return true;
    return false;
}

bool isButtonEventValid(const RE::ButtonEvent* a_event) {
    auto device = a_event->device.get();
    auto keyMask = a_event->GetIDCode();

    if ((device != RE::INPUT_DEVICE::kMouse && device != RE::INPUT_DEVICE::kGamepad) ||
        (device == RE::INPUT_DEVICE::kGamepad && getKeycode(keyMask) != ChargeAttackHandler::Settings::leftButton && getKeycode(keyMask) != ChargeAttackHandler::Settings::rightButton) ||
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

bool ChargeAttackHandler::isAttackEvent(const RE::ButtonEvent* a_event) {
    if (!isButtonEventValid(a_event)) return false;
    
    const auto gameUI = RE::UI::GetSingleton();
    const auto controlMap = RE::ControlMap::GetSingleton();
    if (!gameUI|| !controlMap || gameUI->GameIsPaused() || gameUI->IsMenuOpen(RE::DialogueMenu::MENU_NAME) || gameUI->IsMenuOpen(RE::BarterMenu::MENU_NAME)) return false;
    
    bool isBlocking = false;
    bool isStaggered = false;
    player->GetGraphVariableBool("IsBlocking", isBlocking);
    player->GetGraphVariableBool("IsStaggering", isStaggered);
    if (player->IsInKillMove() || player->IsOnMount() || player->IsInMidair() || player->IsInRagdollState() ||
        isPlayerAttacking(player) || isBlocking || isStaggered || player->IsInWater())
    {
        return false;
    }

    auto playerState = player->AsActorState();
    if (!playerState || playerState->GetWeaponState() != RE::WEAPON_STATE::kDrawn ||
        playerState->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal ||
        playerState->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal ||
        playerState->GetFlyState() != RE::FLY_STATE::kNone)
    {
        return false;
    }

    auto isLeft = isLeftButton(a_event);
    auto weapon = player->GetEquippedObject(isLeft)->As<RE::TESObjectWEAP>();
    return isWeaponValid(weapon, isLeft);
}

void ChargeAttackHandler::performPowerAttack(bool isDualAttack) {
    std::string animationEvent;

    animationEvent = "attackPowerStanding";
    //damageMultiplier = 1.2f;
    
    // Store damage multiplier for hit event
    //StoreAttackData(kDamageMult);

    if (isDualAttack) {
        animationEvent = "attackPowerDual";
        kDamageMult += 0.5f;
    } else {
        animationEvent = "attackPowerStanding";
    }
    
    player->NotifyAnimationGraph(animationEvent);
}

void ChargeAttackHandler::performNormalAttack(bool isLeft, bool isDualAttack) {
    SKSE::log::info("Executing normal attack");
        
    if (isDualAttack) {
        // Dual wield normal attack
        player->NotifyAnimationGraph("attackStart");
    } else {
        // Single weapon normal attack
        std::string animEvent = isLeft ? "attackStartLeft" : "attackStartRight";
        player->NotifyAnimationGraph(animEvent);
    }
}

void ChargeAttackHandler::charge(float holdTime) {
    if (holdTime < Settings::minHoldTime) return;

    // damage stamina
    player->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina, -0.5f);

    // increase damage
    kDamageMult += 0.1f;
}

void ChargeAttackHandler::processHold(RE::ButtonEvent *button) {
    auto isLeft = isLeftButton(button);
    if (isLeft) {
        leftHoldTime = button->HeldDuration();
    } else {
        rightHoldTime = button->HeldDuration();
    }

    float holdTime = isLeft ? leftHoldTime : rightHoldTime;
    if (!bCharging) {
        bCharging = true;

        // Apply slow effect
        auto slowSpell = CreateSlowSpell();
        auto caster = player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
        caster->CastSpellImmediate(slowSpell, false, player, 1.0f, false, 0.0f, nullptr);
    } else {
        charge(holdTime);
    }
}

void ChargeAttackHandler::processRelease(RE::ButtonEvent *button) {
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
        if (isDualAttack) {
            leftRelease = 0;
            rightRelease = 0;
        }

        if (bCharging) {
            performPowerAttack(isDualAttack);
        } else {
            performNormalAttack(isLeft, isDualAttack);
        }
    }

    // Cleanup
    if (bCharging) {
        bCharging = false;

        // remove slowdown
        RE::BSPointerHandle<RE::Actor> nullHandle;
        player->GetMagicTarget()->DispelEffect(CreateSlowSpell(), nullHandle);
    }
}

void ChargeAttackHandler::ProcessButton(RE::ButtonEvent* a_event, void* a_data) {
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

void ChargeAttackHandler::Hook() {
    REL::Relocation<uintptr_t> vtable{ RE::VTABLE_AttackBlockHandler[0] };
    FnProcessButton fn = SKSE::stl::unrestricted_cast<FnProcessButton>(vtable.write_vfunc(4, &ChargeAttackHandler::ProcessButton));
    GetFnHash<FnProcessButton>().insert(std::pair<uintptr_t, FnProcessButton>(vtable.address(), fn));
}

void ChargeAttackHandler::initialise() {
    player = RE::PlayerCharacter::GetSingleton();
    tasks = SKSE::GetTaskInterface();
    loadSettings();

    SKSE::log::info("Data initialised...");
    Hook();
}

void ChargeAttackHandler::loadSettings() {
    constexpr auto path = L"Data/SKSE/Plugins/ChargeAttackNG.ini";

    CSimpleIniA ini;
    ini.SetUnicode();
    ini.LoadFile(path);

    ChargeAttackHandler::Settings::leftButton = ini.GetLongValue("Input", "leftButton", 280);
    ChargeAttackHandler::Settings::rightButton = ini.GetLongValue("Input", "rightButton", 281);
    ChargeAttackHandler::Settings::isMouseReverse = ini.GetBoolValue("Input", "reverseMouseButtons", false);
    ChargeAttackHandler::Settings::minHoldTime = ini.GetDoubleValue("Input", "minimumHoldTime", 0.44f);
    // Stamina drain rate
    // damage scale rate
    // animation playback

    ini.SetLongValue("Input", "leftButton", ChargeAttackHandler::Settings::leftButton);
    ini.SetLongValue("Input", "rightButton", ChargeAttackHandler::Settings::rightButton);
    ini.SetBoolValue("Input", "reverseMouseButtons", ChargeAttackHandler::Settings::isMouseReverse);
    ini.SetDoubleValue("Gameplay", "minimumHoldTime", ChargeAttackHandler::Settings::minHoldTime);

    (void)ini.SaveFile(path);
}
