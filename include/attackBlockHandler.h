#pragma once

#include "utils.h"

/*
This class handles the trampoline hooks in the AttackBlockHandler.
*/
class HookAttackBlockHandler {
public:
    static inline const SKSE::TaskInterface* tasks = nullptr;

    // INI Settings
    struct Settings {
        static inline uint64_t leftButton = 280;
        static inline uint64_t rightButton = 281;
        static inline bool isMouseReverse = false;
        // stamina drain rate
        // attack boost rate
    };

	typedef void (HookAttackBlockHandler::* FnProcessButton) (RE::ButtonEvent*, void*);
    typedef void (HookAttackBlockHandler::* FnUpdateHeldStateActive) (const RE::ButtonEvent*);

    static void initialise();
    static void loadSettings();
    void UpdateHeldStateActive(const RE::ButtonEvent* a_event);
	static void Hook();
private:
    static inline RE::PlayerCharacter* player = nullptr;

    // BGS Action IDs
    static inline RE::BGSAction* actionRightAttack = nullptr;
    static inline RE::BGSAction* actionLeftAttack = nullptr;
    static inline RE::BGSAction* actionDualAttack = nullptr;
    static inline RE::BGSAction* actionRightPowerAttack = nullptr;
    static inline RE::BGSAction* actionLeftPowerAttack = nullptr;
    static inline RE::BGSAction* actionDualPowerAttack = nullptr;
    static inline RE::BGSAction* actionLeftRelease = nullptr;
    static inline RE::BGSAction* actionRightRelease = nullptr;

    void beginCharge(const RE::ButtonEvent *button);
};