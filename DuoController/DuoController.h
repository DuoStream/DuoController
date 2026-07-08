// Copyright 2026 Black-Seraph
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <roapi.h>

/// <summary>
/// Controller types supported by DuoController.
/// </summary>
typedef enum _DUO_CONTROLLER_TYPE
{
	/// <summary>
	/// Virtual Xbox One controller (translated to xinput via xinputhid.sys).
	/// </summary>
	DuoControllerTypeXbox = 0,

	/// <summary>
	/// Virtual DualShock 4 controller.
	/// </summary>
	DuoControllerTypeDualShock4 = 1,

	/// <summary>
	/// Virtual DualSense controller.
	/// </summary>
	DuoControllerTypeDualSense = 2,

	/// <summary>
	/// Virtual DualSense Edge controller.
	/// </summary>
	DuoControllerTypeDualSenseEdge = 3
} DUO_CONTROLLER_TYPE;

/// <summary>
/// Defines the populated fields in a force feedback report.
/// </summary>
typedef enum _SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAGS
{
	SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_RIGHT_MOTOR_VALID = 0x1,
	SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_LEFT_MOTOR_VALID = 0x2,
	SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_RIGHT_TRIGGER_VALID = 0x4,
	SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_LEFT_TRIGGER_VALID = 0x8,
} SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAGS;

/// <summary>
/// The power state of the DualSense controller's battery.
/// </summary>
typedef enum _POWER_STATE {
	Discharging = 0x00, // Use PowerPercent
	Charging = 0x01, // Use PowerPercent
	Complete = 0x02, // PowerPercent not valid? assume 100%?
	AbnormalVoltage = 0x0A, // PowerPercent not valid?
	AbnormalTemperature = 0x0B, // PowerPercent not valid?
	ChargingError = 0x0F  // PowerPercent not valid?
} POWER_STATE;

/// <summary>
/// The mute light settings for the DualSense controller.
/// </summary>
typedef enum _MUTE_LIGHT {
	Off = 0,
	On,
	Breathing,
	DoNothing
} MUTE_LIGHT;

/// <summary>
/// The light fade animation settings for the DualSense controller's RGB LED.
/// </summary>
typedef enum _LIGHT_FADE_ANIMATION {
	Nothing = 0,
	FadeIn, // from black to blue
	FadeOut // from blue to black
} LIGHT_FADE_ANIMATION;

/// <summary>
/// The light brightness settings for the DualSense controller's RGB LED.
/// </summary>
typedef enum _LIGHT_BRIGHTNESS {
	Bright = 0,
	Mid,
	Dim
} LIGHT_BRIGHTNESS;

#pragma pack(push, 1)

/// <summary>
/// The Xbox controller input report structure, including Elite paddle buttons.
/// Works for both regular and paddle-equipped XBOX gamepads.
/// </summary>
typedef struct _DUO_CONTROLLER_INPUT_REPORT_XBOX
{
	UINT8 Sync : 1; // Unused
	UINT8 Guide : 1;
	UINT8 Start : 1;
	UINT8 Back : 1;

	UINT8 A : 1;
	UINT8 B : 1;
	UINT8 X : 1;
	UINT8 Y : 1;

	UINT8 DPad : 4; // 0=None, 1=North, 2=Northeast, ..., 8=Northwest (clockwise)

	UINT8 LeftBumper : 1;
	UINT8 RightBumper : 1;
	UINT8 LeftStick : 1;
	UINT8 RightStick : 1;

	UINT8 LeftTrigger; // Analog 0-255
	UINT8 RightTrigger; // Analog 0-255

	UINT16 LeftStickHorizontal; // 0 (far left) to 65535 (far right)
	UINT16 LeftStickVertical;   // 0 (far top) to 65535 (far bottom)
	UINT16 RightStickHorizontal; // 0 (far left) to 65535 (far right)
	UINT16 RightStickVertical; // 0 (far top) to 65535 (far bottom)

	UINT8 Paddle1 : 1;
	UINT8 Paddle2 : 1;
	UINT8 Paddle3 : 1;
	UINT8 Paddle4 : 1;
	UINT8 : 4;

	UINT8 Reserved[20];
} DUO_CONTROLLER_INPUT_REPORT_XBOX;

/// <summary>
/// DualSense touch finger data.
/// </summary>
typedef struct _DS_TOUCH_FINGER_DATA
{
	UINT32 Index : 7;
	UINT32 NotTouching : 1;
	UINT32 FingerX : 12;
	UINT32 FingerY : 12;
} DS_TOUCH_FINGER_DATA;

/// <summary>
/// DualSense input report touch data block.
/// </summary>
typedef struct _DS_TOUCH_DATA
{
	DS_TOUCH_FINGER_DATA Finger[2];
	UINT8 Timestamp;
} DS_TOUCH_DATA;

/// <summary>
/// The DualSense Edge controller input report structure.
/// </summary>
typedef struct _DUO_CONTROLLER_INPUT_REPORT_DUALSENSE
{
	UINT8 LeftStickHorizontal;
	UINT8 LeftStickVertical;
	UINT8 RightStickHorizontal;
	UINT8 RightStickVertical;
	UINT8 LeftTrigger;
	UINT8 RightTrigger;
	UINT8 SeqNo;

	UINT8 DPad : 4;
	UINT8 Square : 1;
	UINT8 Cross : 1;
	UINT8 Circle : 1;
	UINT8 Triangle : 1;

	UINT8 L1 : 1;
	UINT8 R1 : 1;
	UINT8 L2 : 1;
	UINT8 R2 : 1;
	UINT8 Create : 1;
	UINT8 Options : 1;
	UINT8 L3 : 1;
	UINT8 R3 : 1;

	UINT8 Home : 1;
	UINT8 Touchpad : 1;
	UINT8 Mute : 1;
	UINT8 Reserved1 : 1;
	UINT8 LeftFunction : 1;
	UINT8 RightFunction : 1;
	UINT8 LeftPaddle : 1;
	UINT8 RightPaddle : 1;

	UINT8 Reserved2;
	UINT32 ReservedCounter;

	INT16 AngularVelocityX;
	INT16 AngularVelocityY;
	INT16 AngularVelocityZ;

	INT16 AccelerometerX;
	INT16 AccelerometerY;
	INT16 AccelerometerZ;

	UINT32 SensorTimestamp;
	INT8 Temperature;

	DS_TOUCH_DATA TouchData;

	UINT8 RightTriggerStopLocation : 4;
	UINT8 RightTriggerStatus : 4;

	UINT8 LeftTriggerStopLocation : 4;
	UINT8 LeftTriggerStatus : 4;

	UINT32 HostTimestamp;

	UINT8 RightTriggerEffect : 4;
	UINT8 LeftTriggerEffect : 4;

	UINT32 DeviceTimeStamp;

	UINT8 PowerPercent : 4;
	UINT8 PowerState : 4;

	UINT8 PluggedHeadphones : 1;
	UINT8 PluggedMic : 1;
	UINT8 MicMuted : 1;
	UINT8 PluggedUsbData : 1;
	UINT8 PluggedUsbPower : 1;
	UINT8 UsbPowerOnBT : 1;
	UINT8 DockDetect : 1;
	UINT8 PluggedUnk : 1;

	UINT8 PluggedExternalMic : 1;
	UINT8 HapticLowPassFilter : 1;
	UINT8 Reserved3 : 6;

	UINT8 AesCmac[8];
} DUO_CONTROLLER_INPUT_REPORT_DUALSENSE;

/// <summary>
/// The DualShock 4 controller input report structure.
/// </summary>
typedef struct _DUO_CONTROLLER_INPUT_REPORT_DS4
{
	UINT8 LeftStickHorizontal;
	UINT8 LeftStickVertical;
	UINT8 RightStickHorizontal;
	UINT8 RightStickVertical;

	UINT8 LeftTrigger;
	UINT8 RightTrigger;

	UINT8 DPad; // 0 = Up, 1 = Up-Right, 2 = Right, 3 = Down-Right, 4 = Down, 5 = Down-Left, 6 = Left, 7 = Up-Left, 8 = Neutral

	UINT8 Square : 1;
	UINT8 Cross : 1;
	UINT8 Circle : 1;
	UINT8 Triangle : 1;
	UINT8 L1 : 1;
	UINT8 R1 : 1;
	UINT8 L2 : 1;
	UINT8 R2 : 1;
	UINT8 Share : 1;
	UINT8 Options : 1;
	UINT8 L3 : 1;
	UINT8 R3 : 1;
	UINT8 PS : 1;
	UINT8 Touchpad : 1;

	INT16 AngularVelocityX;
	INT16 AngularVelocityY;
	INT16 AngularVelocityZ;

	INT16 AccelerometerX;
	INT16 AccelerometerY;
	INT16 AccelerometerZ;

	DS_TOUCH_DATA TouchData;

	/*
	UINT8 Touch1Active;
	USHORT Touch1X;
	USHORT Touch1Y;
	UINT8 Touch2Active;
	USHORT Touch2X;
	USHORT Touch2Y;
	*/
} DUO_CONTROLLER_INPUT_REPORT_DS4;

typedef struct _DUO_CONTROLLER_OUTPUT_REPORT_DS {
	/*    */ // Report ID
	/*    */ // Must be 0x02 for DualSense output reports
	UINT8 ReportId;
	/*    */ // Report Set Flags
	/*    */ // These flags are used to indicate what contents from this report should be processed
	/* 0.0*/ UINT8 EnableRumbleEmulation : 1; // Suggest halving rumble strength
	/* 0.1*/ UINT8 UseRumbleNotHaptics : 1; // 
	/*    */
	/* 0.2*/ UINT8 AllowRightTriggerFFB : 1; // Enable setting RightTriggerFFB
	/* 0.3*/ UINT8 AllowLeftTriggerFFB : 1;  // Enable setting LeftTriggerFFB
	/*    */
	/* 0.4*/ UINT8 AllowHeadphoneVolume : 1; // Enable setting VolumeHeadphones
	/* 0.5*/ UINT8 AllowSpeakerVolume : 1;   // Enable setting VolumeSpeaker
	/* 0.6*/ UINT8 AllowMicVolume : 1;       // Enable setting VolumeMic
	/*    */
	/* 0.7*/ UINT8 AllowAudioControl : 1; // Enable setting AudioControl section
	/* 1.0*/ UINT8 AllowMuteLight : 1;    // Enable setting MuteLightMode
	/* 1.1*/ UINT8 AllowAudioMute : 1;    // Enable setting MuteControl section
	/*    */
	/* 1.2*/ UINT8 AllowLedColor : 1; // Enable RGB LED section
	/*    */
	/* 1.3*/ UINT8 ResetLights : 1; // Release the LEDs from Wireless firmware control
	/*    */                         // When in wireless mode this must be signaled to control LEDs
	/*    */                         // This cannot be applied during the BT pair animation.
	/*    */                         // SDL2 waits until the SensorTimestamp value is >= 10200000
	/*    */                         // before pulsing this bit once.
	/*    */
	/* 1.4*/ UINT8 AllowPlayerIndicators : 1; // Enable setting PlayerIndicators section
	/* 1.5*/ UINT8 AllowHapticLowPassFilter : 1; // Enable HapticLowPassFilter
	/* 1.6*/ UINT8 AllowMotorPowerLevel : 1; // MotorPowerLevel reductions for trigger/haptic
	/* 1.7*/ UINT8 AllowAudioControl2 : 1; // Enable setting AudioControl2 section
	/*    */
	/* 2  */ UINT8 RumbleEmulationRight; // emulates the light weight
	/* 3  */ UINT8 RumbleEmulationLeft; // emulated the heavy weight
	/*    */
	/* 4  */ UINT8 VolumeHeadphones; // max 0x7f
	/* 5  */ UINT8 VolumeSpeaker; // PS5 appears to only use the range 0x3d-0x64
	/* 6  */ UINT8 VolumeMic; // not linear, seems to max at 64, 0 is fully muted only in chat mode
	/*    */
	/*    */ // AudioControl
	/* 7.0*/ UINT8 MicSelect : 2; // 0 Auto
	/*    */                       // 1 Internal Only
	/*    */                       // 2 External Only
	/*    */                       // 3 Unclear, sets external mic flag but might use internal mic, do test
	/* 7.2*/ UINT8 EchoCancelEnable : 1;
	/* 7.3*/ UINT8 NoiseCancelEnable : 1;
	/* 7.4*/ UINT8 OutputPathSelect : 2; // 0 L_R_X
	/*    */                              // 1 L_L_X
	/*    */                              // 2 L_L_R
	/*    */                              // 3 X_X_R
	/* 7.6*/ UINT8 InputPathSelect : 2;  // 0 CHAT_ASR
	/*    */                              // 1 CHAT_CHAT
	/*    */                              // 2 ASR_ASR
	/*    */                              // 3 Does Nothing, invalid
	/*    */
	/* 8  */ UINT8 MuteLightMode; // MUTE_LIGHT
	/*    */
	/*    */ // MuteControl
	/* 9.0*/ UINT8 TouchPowerSave : 1;
	/* 9.1*/ UINT8 MotionPowerSave : 1;
	/* 9.2*/ UINT8 HapticPowerSave : 1; // AKA BulletPowerSave
	/* 9.3*/ UINT8 AudioPowerSave : 1;
	/* 9.4*/ UINT8 MicMute : 1;
	/* 9.5*/ UINT8 SpeakerMute : 1;
	/* 9.6*/ UINT8 HeadphoneMute : 1;
	/* 9.7*/ UINT8 HapticMute : 1; // AKA BulletMute
	/*    */
	/*10  */ UINT8 RightTriggerFFB[11];
	/*21  */ UINT8 LeftTriggerFFB[11];
	/*32  */ UINT32 HostTimestamp; // mirrored into report read
	/*    */
	/*    */ // MotorPowerLevel
	/*36.0*/ UINT8 TriggerMotorPowerReduction : 4; // 0x0-0x7 (no 0x8?) Applied in 12.5% reductions
	/*36.4*/ UINT8 RumbleMotorPowerReduction : 4;  // 0x0-0x7 (no 0x8?) Applied in 12.5% reductions
	/*    */
	/*    */ // AudioControl2
	/*37.0*/ UINT8 SpeakerCompPreGain : 3; // additional speaker volume boost
	/*37.3*/ UINT8 BeamformingEnable : 1; // Probably for MIC given there's 2, might be more bits, can't find what it does
	/*37.4*/ UINT8 UnkAudioControl2 : 4; // some of these bits might apply to the above
	/*    */
	/*38.0*/ UINT8 AllowLightBrightnessChange : 1; // LED_BRIHTNESS_CONTROL
	/*38.1*/ UINT8 AllowColorLightFadeAnimation : 1; // LIGHTBAR_SETUP_CONTROL
	/*38.2*/ UINT8 EnableImprovedRumbleEmulation : 1; // Use instead of EnableRumbleEmulation
	// requires FW >= 0x0224
	// No need to halve rumble strength
	/*38.3*/ UINT8 UNKBITC : 5; // unused
	/*    */
	/*39.0*/ UINT8 HapticLowPassFilter : 1;
	/*39.1*/ UINT8 UNKBIT : 7;
	/*    */
	/*40  */ UINT8 UNKBYTE; // previous notes suggested this was HLPF, was probably off by 1
	/*    */
	/*41  */ UINT8 LightFadeAnimation; // LIGHT_FADE_ANIMATION
	/*42  */ UINT8 LightBrightness; // LIGHT_BRIGHTNESS
	/*    */
	/*    */ // PlayerIndicators
	/*    */ // These bits control the white LEDs under the touch pad.
	/*    */ // Note the reduction in functionality for later revisions.
	/*    */ // Generation 0x03 - Full Functionality
	/*    */ // Generation 0x04 - Mirrored Only
	/*    */ // Suggested detection: (HardwareInfo & 0x00FFFF00) == 0X00000400
	/*    */ //
	/*    */ // Layout used by PS5:
	/*    */ // 0x04 - -x- -  Player 1
	/*    */ // 0x06 - x-x -  Player 2
	/*    */ // 0x15 x -x- x  Player 3
	/*    */ // 0x1B x x-x x  Player 4
	/*    */ // 0x1F x xxx x  Player 5* (Unconfirmed)
	/*    */ //
	/*    */ //                        // HW 0x03 // HW 0x04
	/*43.0*/ UINT8 PlayerLight1 : 1; // x --- - // x --- x
	/*43.1*/ UINT8 PlayerLight2 : 1; // - x-- - // - x-x -
	/*43.2*/ UINT8 PlayerLight3 : 1; // - -x- - // - -x- -
	/*43.3*/ UINT8 PlayerLight4 : 1; // - --x - // - x-x -
	/*43.4*/ UINT8 PlayerLight5 : 1; // - --- x // x --- x
	/*43.5*/ UINT8 PlayerLightFade : 1; // if low player lights fade in, if high player lights instantly change
	/*43.6*/ UINT8 PlayerLightUNK : 2;
	/*    */
	/*    */ // RGB LED
	/*44  */ UINT8 LedRed;
	/*45  */ UINT8 LedGreen;
	/*46  */ UINT8 LedBlue;
} DUO_CONTROLLER_OUTPUT_REPORT_DS;

/// <summary>
/// The controller force feedback report structure.
/// </summary>
typedef struct _DUO_CONTROLLER_FORCE_FEEDBACK_REPORT
{
	UINT8 Flags; // SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_RIGHT_MOTOR_VALID | SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_LEFT_MOTOR_VALID | SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_RIGHT_TRIGGER_VALID | SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_LEFT_TRIGGER_VALID
	UINT8 LeftTrigger; // 0-255
	UINT8 RightTrigger; // 0-255
	UINT8 LeftMotor; // 0-255
	UINT8 RightMotor; // 0-255
	UINT8 Duration; // 0-255 (246 on test capture, must be 255 when combined with Delay)
	UINT8 StartDelay; // 0-255 (9 on test capture, must be 255 when combined with Duration)
	UINT8 Loop; // 0 or 1 (0 on test capture)
} DUO_CONTROLLER_FORCE_FEEDBACK_REPORT;

#pragma pack(pop)

/// <summary>
/// Receives vibration data from a Duo controller.
/// </summary>
/// <param name="controller">The controller</param>
/// <param name="report">The force feedback report</param>
/// <param name="context">The context</param>
typedef void (*DuoController_VibrationReportCallback_t)(void* controller, DUO_CONTROLLER_FORCE_FEEDBACK_REPORT* report, void* context);

/// <summary>
/// Initializes the DuoController library.
/// </summary>
/// <returns>Result</returns>
HRESULT WINAPI DuoController_Initialize();

/// <summary>
/// Uninitializes the DuoController library.
/// </summary>
/// <returns>Result</returns>
HRESULT WINAPI DuoController_Uninitialize();

/// <summary>
/// Creates a new Duo controller.
/// </summary>
/// <param name="controllerType">The type of controller to create</param>
/// <param name="vibrationCallback">The vibration report callback</param>
/// <param name="vibrationCallbackContext">The vibration callback context</param>
/// <param name="controller">Receives the created controller</param>
/// <returns>Result</returns>
HRESULT WINAPI DuoController_CreateController(DUO_CONTROLLER_TYPE controllerType, DuoController_VibrationReportCallback_t vibrationCallback, void* vibrationCallbackContext, void** controller);

/// <summary>
/// Removes a Duo controller.
/// </summary>
/// <param name="controller">The controller to remove</param>
/// <param name="inputReport">The controller-specific input report to send</param>
/// <returns>Result</returns>
HRESULT WINAPI DuoController_RemoveController(void* controller);

/// <summary>
/// Sends an input report to the given Duo controller.
/// The report type is determined by the controller type passed to DuoController_CreateController.
/// </summary>
/// <param name="controller">The controller to send the input report to</param>
/// <param name="inputReport">The controller-specific input report</param>
/// <returns>Result</returns>
HRESULT WINAPI DuoController_SendReport(void* controller, void* inputReport);
