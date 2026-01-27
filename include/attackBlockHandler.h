#pragma once

#include "utils.h"

/*
This class handles the trampoline hooks in the AttackBlockHandler class.
*/
class HookAttackBlockHandler {
public:
    static inline const SKSE::TaskInterface* tasks = nullptr;

    // INI Settings
    struct Settings {
        static inline uint64_t leftButton = 280;
        static inline uint64_t rightButton = 281;
        static inline bool isMouseReverse = false;
        static inline float minHoldTime = 0.44f;
        // stamina drain rate
        // attack boost rate
    };

	typedef void (HookAttackBlockHandler::* FnProcessButton) (RE::ButtonEvent*, void*);

    static void initialise();
    static void loadSettings();
    void ProcessButton(RE::ButtonEvent* a_event, void* a_data);
	static void Hook();
private:
    static inline RE::PlayerCharacter* player = nullptr;

    // BGS Action IDs
    static inline RE::BGSAction* actionRightPowerAttack = nullptr;
    static inline RE::BGSAction* actionLeftPowerAttack = nullptr;
    static inline RE::BGSAction* actionDualPowerAttack = nullptr;
    static inline RE::BGSAction* actionRightAttack = nullptr;
    static inline RE::BGSAction* actionLeftAttack = nullptr;
    static inline RE::BGSAction* actionDualAttack = nullptr;

    bool isAttackEvent(const RE::ButtonEvent* a_event);
    void setIndication(bool isLeft, bool val);
    RE::BGSAction* getAttackAction(bool isLeft, bool isDualAttack, float holdTime);

    void charge(float holdTime);
    void performAction(RE::BGSAction* action, RE::Actor* actor);
    void processHold(RE::ButtonEvent *button);
    void processRelease(RE::ButtonEvent *button);
};