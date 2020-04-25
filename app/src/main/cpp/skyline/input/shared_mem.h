// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <array>
#include <common.h>

namespace skyline {
    namespace constant {
        constexpr u8 HidEntryCount = 17; //!< The amount of state entries in each hid device
    }

    namespace input {
        struct CommonHeader {
            u64 timestamp;
            u64 entryCount;
            u64 currentEntry;
            u64 maxEntry;
        };
        static_assert(sizeof(CommonHeader) == 0x20);



        struct DebugPadState {
            u64 timestamp;
            u8 _unk_[0x20];
        };
        static_assert(sizeof(DebugPadState) == 0x28);

        struct DebugPadSection {
            CommonHeader header;
            std::array<DebugPadState, constant::HidEntryCount> state;
            u64 _pad_[0x27];
        };
        static_assert(sizeof(DebugPadSection) == 0x400);



        struct TouchScreenStateData {
            u64 timestamp;
            u32 _pad0_;

            u32 index;

            u32 positionX;
            u32 positionY;

            u32 diameterX;
            u32 diameterY;

            u32 angle;
            u32 _pad1_;
        };
        static_assert(sizeof(TouchScreenStateData) == 0x28);

        struct TouchScreenState {
            u64 globalTimestamp;
            u64 localTimestamp;

            u64 touchCount;
            std::array<TouchScreenStateData, 16> data;
        };
        static_assert(sizeof(TouchScreenState) == 0x298);

        struct TouchScreenSection {
            CommonHeader header;
            std::array<TouchScreenState, constant::HidEntryCount> state;
            u64 _pad_[0x79];

        };
        static_assert(sizeof(TouchScreenSection) == 0x3000);



        struct MouseState {
            u64 globalTimestamp;
            u64 localTimestamp;

            u32 positionX;
            u32 positionY;

            u32 changeX;
            u32 changeY;

            u32 scrollChangeY;
            u32 scrollChangeX;

            u64 buttons;
        };
        static_assert(sizeof(MouseState) == 0x30);

        struct MouseSection {
            CommonHeader header;
            std::array<MouseState, constant::HidEntryCount> state;
            u64 _pad_[22];
        };
        static_assert(sizeof(MouseSection) == 0x400);



        struct KeyboardState {
            u64 globalTimestamp;
            u64 localTimestamp;

            u64 modifers;
            u64 keysDown[4];
        };
        static_assert(sizeof(KeyboardState) == 0x38);

        struct KeyboardSection {
            CommonHeader header;
            std::array<KeyboardState, constant::HidEntryCount> state;
            u64 _pad_[5];
        };
        static_assert(sizeof(KeyboardSection) == 0x400);



        struct BasicXpadState {
            u64 globalTimestamp;
            u64 _unk_[4];
        };
        static_assert(sizeof(BasicXpadState) == 0x28);

        struct BasicXpadSection {
            CommonHeader header;
            std::array<BasicXpadState, constant::HidEntryCount> state;
            u64 _pad_[39];
        };
        static_assert(sizeof(BasicXpadSection) == 0x400);



        struct HomeButtonState {
            u64 globalTimestamp;
            u64 _unk_[2];
        };
        static_assert(sizeof(HomeButtonState) == 0x18);

        struct HomeButtonSection {
            CommonHeader header;
            std::array<HomeButtonState, constant::HidEntryCount> state;
            u64 _pad_[9];
        };
        static_assert(sizeof(HomeButtonSection) == 0x200);



        struct SleepButtonState {
            u64 globalTimestamp;
            u64 _unk_[2];
        };
        static_assert(sizeof(SleepButtonState) == 0x18);

        struct SleepButtonSection {
            CommonHeader header;
            std::array<SleepButtonState, constant::HidEntryCount> state;
            u64 _pad_[9];
        };
        static_assert(sizeof(SleepButtonSection) == 0x200);



        struct CaptureButtonState {
            u64 globalTimestamp;
            u64 _unk_[2];
        };
        static_assert(sizeof(CaptureButtonState) == 0x18);

        struct CaptureButtonSection {
            CommonHeader header;
            std::array<CaptureButtonState, constant::HidEntryCount> state;
            u64 _pad_[9];
        };
        static_assert(sizeof(CaptureButtonSection) == 0x200);


        struct InputDetectorState {
            u64 globalTimestamp;
            u64 _unk_[2];
        };
        static_assert(sizeof(InputDetectorState) == 0x18);

        struct InputDetectorSection {
            CommonHeader header;
            std::array<InputDetectorState, 2> state;
            u64 _pad_[6];
        };
        static_assert(sizeof(InputDetectorSection) == 0x80);


        struct GestureState {
            u64 globalTimestamp;
            u64 _unk_[12];
        };
        static_assert(sizeof(GestureState) == 0x68);

        struct GestureSection {
            CommonHeader header;
            std::array<GestureState, constant::HidEntryCount> state;
            u64 _pad_[31];
        };
        static_assert(sizeof(GestureSection) == 0x800);

        struct ConsoleSixAxisSensorSection {
            u64 timestamp;
            bool resting : 1;
            u32 _pad0_ : 3;
            u32 verticalizationError;
            u32 gyroBias[3];
            u32 _pad1_;
        };
        static_assert(sizeof(ConsoleSixAxisSensorSection) == 0x20);
    }
}
