#pragma once

namespace {
    uint32_t getKeycode(uint32_t dxScanCode) {
        int dxGamepadKeycode = -1;
        RE::BSWin32GamepadDevice::Key gamepadKey = static_cast<RE::BSWin32GamepadDevice::Key>(dxScanCode);
        switch (gamepadKey) {
            case RE::BSWin32GamepadDevice::Key::kUp:
                dxGamepadKeycode = 266;
                break;
            case RE::BSWin32GamepadDevice::Key::kDown:
                dxGamepadKeycode = 267;
                break;
            case RE::BSWin32GamepadDevice::Key::kLeft:
                dxGamepadKeycode = 268;
                break;
            case RE::BSWin32GamepadDevice::Key::kRight:
                dxGamepadKeycode = 269;
                break;
            case RE::BSWin32GamepadDevice::Key::kStart:
                dxGamepadKeycode = 270;
                break;
            case RE::BSWin32GamepadDevice::Key::kBack:
                dxGamepadKeycode = 271;
                break;
            case RE::BSWin32GamepadDevice::Key::kLeftThumb:
                dxGamepadKeycode = 272;
                break;
            case RE::BSWin32GamepadDevice::Key::kRightThumb:
                dxGamepadKeycode = 273;
                break;
            case RE::BSWin32GamepadDevice::Key::kLeftShoulder:
                dxGamepadKeycode = 274;
                break;
            case RE::BSWin32GamepadDevice::Key::kRightShoulder:
                dxGamepadKeycode = 275;
                break;
            case RE::BSWin32GamepadDevice::Key::kA:
                dxGamepadKeycode = 276;
                break;
            case RE::BSWin32GamepadDevice::Key::kB:
                dxGamepadKeycode = 277;
                break;
            case RE::BSWin32GamepadDevice::Key::kX:
                dxGamepadKeycode = 278;
                break;
            case RE::BSWin32GamepadDevice::Key::kY:
                dxGamepadKeycode = 279;
                break;
            case RE::BSWin32GamepadDevice::Key::kLeftTrigger:
                dxGamepadKeycode = 280;
                break;
            case RE::BSWin32GamepadDevice::Key::kRightTrigger:
                dxGamepadKeycode = 281;
                break;
            default:
                dxGamepadKeycode = static_cast<uint32_t>(-1);
                break;
        }
        return dxGamepadKeycode;
    }
    
    uint64_t timestamp() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }
}
