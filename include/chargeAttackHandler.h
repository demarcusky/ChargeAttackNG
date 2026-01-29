#pragma once

#include "utils.h"

class ChargeAttackHandler {
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

	typedef void (ChargeAttackHandler::* FnProcessButton) (RE::ButtonEvent*, void*);

    static void initialise();
    static void loadSettings();
	static void Hook();
    void ProcessButton(RE::ButtonEvent* a_event, void* a_data); // AttackBlockHandler Hook
private:
    static inline RE::PlayerCharacter* player = nullptr;

    bool isAttackEvent(const RE::ButtonEvent* a_event);
    void performPowerAttack(bool isDualAttack);
    void performNormalAttack(bool isLeft, bool isDualAttack);
    void charge(float holdTime);
    void stopCharge();
    void processHold(RE::ButtonEvent *button);
    void processRelease(RE::ButtonEvent *button);
};