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
	/// Virtual Xbox One controller (uses xboxgipsynthetic.dll).
	/// </summary>
	DuoControllerTypeXbox = 0,

	/// <summary>
	/// Virtual DualSense controller (uses DuoController driver).
	/// </summary>
	DuoControllerTypeDs = 1,
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

#pragma pack(push, 1)

/// <summary>
/// The Xbox controller input report structure.
/// </summary>
typedef struct _DUO_CONTROLLER_INPUT_REPORT
{
	UINT8 Sync : 1; // Unused
	UINT8 Guide : 1;
	UINT8 Start : 1;
	UINT8 Back : 1;

	UINT8 A : 1;
	UINT8 B : 1;
	UINT8 X : 1;
	UINT8 Y : 1;

	UINT8 DPadUp : 1;
	UINT8 DPadDown : 1;
	UINT8 DPadLeft : 1;
	UINT8 DPadRight : 1;

	UINT8 LeftBumper : 1;
	UINT8 RightBumper : 1;
	UINT8 LeftStick : 1;
	UINT8 RightStick : 1;

	UINT16 LeftTrigger; // Analog 0-65535
	UINT16 RightTrigger; // Analog 0-65535

	INT16 LeftStickHorizontal; // -32768 (left) to 32767 (right)
	INT16 LeftStickVertical;   // -32768 (down) to 32767 (up)
	INT16 RightStickHorizontal; // -32768 (left) to 32767 (right)
	INT16 RightStickVertical;   // -32768 (down) to 32767 (up)
} DUO_CONTROLLER_INPUT_REPORT;

/// <summary>
/// Touch finger data for a single touch point. Bit-packed per DualSense wire format (4 bytes).
/// </summary>
typedef struct _DS_TOUCH_FINGER_DATA
{
	UINT32 Index : 7;
	UINT32 NotTouching : 1;
	UINT32 FingerX : 12;
	UINT32 FingerY : 12;
} DS_TOUCH_FINGER_DATA;

/// <summary>
/// Touch data block from DualSense input report (9 bytes = 2 finger records + timestamp).
/// </summary>
typedef struct _DS_TOUCH_DATA
{
	DS_TOUCH_FINGER_DATA Finger[2];
	UINT8 Timestamp;
} DS_TOUCH_DATA;

/// <summary>
/// DualSense USB Get Controller State input report (63 bytes, after Report ID 0x01).
/// Matches the USBGetStateData structure from the Game Controller Collective Wiki.
/// Includes DualSense Edge paddle/function buttons.
/// </summary>
typedef struct _DUO_CONTROLLER_INPUT_REPORT_DS
{
	UINT8 LeftStickX;
	UINT8 LeftStickY;
	UINT8 RightStickX;
	UINT8 RightStickY;
	UINT8 TriggerLeft;
	UINT8 TriggerRight;
	UINT8 SeqNo;

	UINT8 DPad : 4;
	UINT8 ButtonSquare : 1;
	UINT8 ButtonCross : 1;
	UINT8 ButtonCircle : 1;
	UINT8 ButtonTriangle : 1;

	UINT8 ButtonL1 : 1;
	UINT8 ButtonR1 : 1;
	UINT8 ButtonL2 : 1;
	UINT8 ButtonR2 : 1;
	UINT8 ButtonCreate : 1;
	UINT8 ButtonOptions : 1;
	UINT8 ButtonL3 : 1;
	UINT8 ButtonR3 : 1;

	UINT8 ButtonHome : 1;
	UINT8 ButtonPad : 1;
	UINT8 ButtonMute : 1;
	UINT8 Reserved1 : 1;
	UINT8 ButtonLeftFunction : 1;
	UINT8 ButtonRightFunction : 1;
	UINT8 ButtonLeftPaddle : 1;
	UINT8 ButtonRightPaddle : 1;

	UINT8 Reserved2;
	UINT32 ReservedCounter;

	INT16 AngularVelocityX;
	INT16 AngularVelocityZ;
	INT16 AngularVelocityY;

	INT16 AccelerometerX;
	INT16 AccelerometerY;
	INT16 AccelerometerZ;

	UINT32 SensorTimestamp;
	INT8 Temperature;

	DS_TOUCH_DATA TouchData;

	UINT8 TriggerRightStopLocation : 4;
	UINT8 TriggerRightStatus : 4;

	UINT8 TriggerLeftStopLocation : 4;
	UINT8 TriggerLeftStatus : 4;

	UINT32 HostTimestamp;

	UINT8 TriggerRightEffect : 4;
	UINT8 TriggerLeftEffect : 4;

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
} DUO_CONTROLLER_INPUT_REPORT_DS;

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
	UINT8 Delay; // 0-255 (9 on test capture, must be 255 when combined with Duration)
	UINT8 Repeat; // 0 or 1 (0 on test capture)
	UINT8 PulsePeriod; // 0 on test capture
	UINT8 NumberOfPulses; // 235 on test capture
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
/// <returns>Result</returns>
HRESULT WINAPI DuoController_RemoveController(void* controller);

/// <summary>
/// Sends an Xbox input report to the given Duo controller.
/// </summary>
/// <param name="controller">The controller to send the input report to</param>
/// <param name="inputReport">The input report to send</param>
/// <returns>Result</returns>
HRESULT WINAPI DuoController_SendReport(void* controller, DUO_CONTROLLER_INPUT_REPORT* inputReport);

/// <summary>
/// Sends a DualSense input report to the given Duo controller.
/// </summary>
/// <param name="controller">The controller to send the input report to</param>
/// <param name="inputReport">The DualSense input report to send</param>
/// <returns>Result</returns>
HRESULT WINAPI DuoController_SendReportDs(void* controller, DUO_CONTROLLER_INPUT_REPORT_DS* inputReport);
