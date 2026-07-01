#include "../DuoController/DuoController.h"
#include <stdio.h>
#include <stdlib.h>

typedef HRESULT (*DuoController_Initialize_t)();
typedef HRESULT (*DuoController_Uninitialize_t)();
typedef HRESULT(*DuoController_CreateController_t)(DUO_CONTROLLER_TYPE controllerType, DuoController_VibrationReportCallback_t vibrationCallback, void* vibrationCallbackContext, void** controller);
typedef HRESULT(*DuoController_RemoveController_t)(void* controller);
typedef HRESULT(*DuoController_SendReport_t)(void* controller, void* inputReport);

/// <summary>
/// Receives vibration reports from the Duo controller.
/// </summary>
void VibrationReportCallback(void* controller, DUO_CONTROLLER_FORCE_FEEDBACK_REPORT* report, void* context)
{
	wprintf(L"Vibration report received: LeftMotor=%d, RightMotor=%d, Duration=%d, Delay=%d, Repeat=%d\n", report->LeftMotor, report->RightMotor, report->Duration, report->Delay, report->Repeat);
}

/// <summary>
/// Runs the Xbox input report loop.
/// </summary>
static void RunXboxLoop(void* controller, DuoController_SendReport_t sendReport)
{
	DUO_CONTROLLER_INPUT_REPORT_XBOX inputReport;
	memset(&inputReport, 0, sizeof(inputReport));
	char lineBuffer[128];

	while (fgets(lineBuffer, sizeof(lineBuffer), stdin) != NULL)
	{
		if (strcmp(lineBuffer, "exit\n") == 0)
			break;

		if (strcmp(lineBuffer, "reset\n") == 0)
		{
			memset(&inputReport, 0, sizeof(inputReport));
			HRESULT hr = sendReport(controller, &inputReport);
			wprintf(SUCCEEDED(hr) ? L"Report sent\n" : L"Send failed: 0x%08X\n", hr);
			continue;
		}

		char buttonName[64];
		int buttonState = 0;
		if (sscanf_s(lineBuffer, "%63s %d", buttonName, (unsigned)_countof(buttonName), &buttonState) != 2)
		{
			wprintf(L"Usage: fieldname value\n");
			continue;
		}

		if (strcmp(buttonName, "Sync") == 0)               inputReport.Sync = buttonState;
		else if (strcmp(buttonName, "Guide") == 0)          inputReport.Guide = buttonState;
		else if (strcmp(buttonName, "Start") == 0)          inputReport.Start = buttonState;
		else if (strcmp(buttonName, "Back") == 0)           inputReport.Back = buttonState;
		else if (strcmp(buttonName, "A") == 0)              inputReport.A = buttonState;
		else if (strcmp(buttonName, "B") == 0)              inputReport.B = buttonState;
		else if (strcmp(buttonName, "X") == 0)              inputReport.X = buttonState;
		else if (strcmp(buttonName, "Y") == 0)              inputReport.Y = buttonState;
		else if (strcmp(buttonName, "DPadUp") == 0)         inputReport.DPadUp = buttonState;
		else if (strcmp(buttonName, "DPadDown") == 0)       inputReport.DPadDown = buttonState;
		else if (strcmp(buttonName, "DPadLeft") == 0)       inputReport.DPadLeft = buttonState;
		else if (strcmp(buttonName, "DPadRight") == 0)      inputReport.DPadRight = buttonState;
		else if (strcmp(buttonName, "LeftBumper") == 0)     inputReport.LeftBumper = buttonState;
		else if (strcmp(buttonName, "RightBumper") == 0)    inputReport.RightBumper = buttonState;
		else if (strcmp(buttonName, "LeftStick") == 0)      inputReport.LeftStick = buttonState;
		else if (strcmp(buttonName, "RightStick") == 0)     inputReport.RightStick = buttonState;
		else if (strcmp(buttonName, "LeftTrigger") == 0)    inputReport.LeftTrigger = (BYTE)buttonState;
		else if (strcmp(buttonName, "RightTrigger") == 0)   inputReport.RightTrigger = (BYTE)buttonState;
		else if (strcmp(buttonName, "LeftStickHorizontal") == 0)  inputReport.LeftStickHorizontal = (SHORT)buttonState;
		else if (strcmp(buttonName, "LeftStickVertical") == 0)    inputReport.LeftStickVertical = (SHORT)buttonState;
		else if (strcmp(buttonName, "RightStickHorizontal") == 0) inputReport.RightStickHorizontal = (SHORT)buttonState;
		else if (strcmp(buttonName, "RightStickVertical") == 0)   inputReport.RightStickVertical = (SHORT)buttonState;
		else if (strcmp(buttonName, "Paddle1") == 0) inputReport.Paddle1 = buttonState;
		else if (strcmp(buttonName, "Paddle2") == 0) inputReport.Paddle2 = buttonState;
		else if (strcmp(buttonName, "Paddle3") == 0) inputReport.Paddle3 = buttonState;
		else if (strcmp(buttonName, "Paddle4") == 0) inputReport.Paddle4 = buttonState;
		else
		{
			wprintf(L"Unknown Xbox field: %hs\n", buttonName);
			continue;
		}

		wprintf(L"Sending report in 3 seconds...\n");
		Sleep(3000);
		HRESULT hr = sendReport(controller, &inputReport);
		wprintf(SUCCEEDED(hr) ? L"Report sent\n" : L"Send failed: 0x%08X\n", hr);
	}
}

/// <summary>
/// Runs the DualSense Edge input report loop.
/// </summary>
static void RunDualSenseLoop(void* controller, DuoController_SendReport_t sendReport)
{
	DUO_CONTROLLER_INPUT_REPORT_DUALSENSE inputReport;
	memset(&inputReport, 0, sizeof(inputReport));
	char lineBuffer[128];

	while (fgets(lineBuffer, sizeof(lineBuffer), stdin) != NULL)
	{
		if (strcmp(lineBuffer, "exit\n") == 0)
			break;

		if (strcmp(lineBuffer, "reset\n") == 0)
		{
			memset(&inputReport, 0, sizeof(inputReport));
			HRESULT hr = sendReport(controller, &inputReport);
			wprintf(SUCCEEDED(hr) ? L"Report sent\n" : L"Send failed: 0x%08X\n", hr);
			continue;
		}

		char fieldName[64];
		int value = 0;
		if (sscanf_s(lineBuffer, "%63s %d", fieldName, (unsigned)_countof(fieldName), &value) != 2)
		{
			wprintf(L"Usage: fieldname value\n");
			continue;
		}

		if (strcmp(fieldName, "LeftStickHorizontal") == 0)      inputReport.LeftStickHorizontal = (BYTE)value;
		else if (strcmp(fieldName, "LeftStickVertical") == 0) inputReport.LeftStickVertical = (BYTE)value;
		else if (strcmp(fieldName, "RightStickHorizontal") == 0) inputReport.RightStickHorizontal = (BYTE)value;
		else if (strcmp(fieldName, "RightStickVertical") == 0) inputReport.RightStickVertical = (BYTE)value;
		else if (strcmp(fieldName, "DPad") == 0)        inputReport.DPad = (BYTE)value;
		else if (strcmp(fieldName, "Square") == 0)  inputReport.Square = (BYTE)value;
		else if (strcmp(fieldName, "Cross") == 0)   inputReport.Cross = (BYTE)value;
		else if (strcmp(fieldName, "Circle") == 0)  inputReport.Circle = (BYTE)value;
		else if (strcmp(fieldName, "Triangle") == 0) inputReport.Triangle = (BYTE)value;
		else if (strcmp(fieldName, "L1") == 0)      inputReport.L1 = (BYTE)value;
		else if (strcmp(fieldName, "R1") == 0)      inputReport.R1 = (BYTE)value;
		else if (strcmp(fieldName, "L2") == 0)      inputReport.L2 = (BYTE)value;
		else if (strcmp(fieldName, "R2") == 0)      inputReport.R2 = (BYTE)value;
		else if (strcmp(fieldName, "Create") == 0)  inputReport.Create = (BYTE)value;
		else if (strcmp(fieldName, "Options") == 0) inputReport.Options = (BYTE)value;
		else if (strcmp(fieldName, "L3") == 0)      inputReport.L3 = (BYTE)value;
		else if (strcmp(fieldName, "R3") == 0)      inputReport.R3 = (BYTE)value;
		else if (strcmp(fieldName, "Home") == 0)    inputReport.Home = (BYTE)value;
		else if (strcmp(fieldName, "Touchpad") == 0)     inputReport.Touchpad = (BYTE)value;
		else if (strcmp(fieldName, "Mute") == 0)    inputReport.Mute = (BYTE)value;
		else if (strcmp(fieldName, "LeftFunction") == 0)  inputReport.LeftFunction = (BYTE)value;
		else if (strcmp(fieldName, "RightFunction") == 0) inputReport.RightFunction = (BYTE)value;
		else if (strcmp(fieldName, "LeftPaddle") == 0)    inputReport.LeftPaddle = (BYTE)value;
		else if (strcmp(fieldName, "RightPaddle") == 0)   inputReport.RightPaddle = (BYTE)value;
		else if (strcmp(fieldName, "LeftTrigger") == 0)   inputReport.LeftTrigger = (BYTE)value;
		else if (strcmp(fieldName, "RightTrigger") == 0)  inputReport.RightTrigger = (BYTE)value;
		else if (strcmp(fieldName, "AngularVelocityX") == 0) inputReport.AngularVelocityX = (INT16)value;
		else if (strcmp(fieldName, "AngularVelocityY") == 0) inputReport.AngularVelocityY = (INT16)value;
		else if (strcmp(fieldName, "AngularVelocityZ") == 0) inputReport.AngularVelocityZ = (INT16)value;
		else if (strcmp(fieldName, "AccelerometerX") == 0)  inputReport.AccelerometerX = (INT16)value;
		else if (strcmp(fieldName, "AccelerometerY") == 0)  inputReport.AccelerometerY = (INT16)value;
		else if (strcmp(fieldName, "AccelerometerZ") == 0)  inputReport.AccelerometerZ = (INT16)value;
		else if (strcmp(fieldName, "TouchFinger1Index") == 0)       inputReport.TouchData.Finger[0].Index = (UINT32)value;
		else if (strcmp(fieldName, "TouchFinger1NotTouching") == 0) inputReport.TouchData.Finger[0].NotTouching = (UINT32)value;
		else if (strcmp(fieldName, "TouchFinger1X") == 0)           inputReport.TouchData.Finger[0].FingerX = (UINT32)value;
		else if (strcmp(fieldName, "TouchFinger1Y") == 0)           inputReport.TouchData.Finger[0].FingerY = (UINT32)value;
		else if (strcmp(fieldName, "TouchFinger2Index") == 0)       inputReport.TouchData.Finger[1].Index = (UINT32)value;
		else if (strcmp(fieldName, "TouchFinger2NotTouching") == 0) inputReport.TouchData.Finger[1].NotTouching = (UINT32)value;
		else if (strcmp(fieldName, "TouchFinger2X") == 0)           inputReport.TouchData.Finger[1].FingerX = (UINT32)value;
		else if (strcmp(fieldName, "TouchFinger2Y") == 0)           inputReport.TouchData.Finger[1].FingerY = (UINT32)value;
		else if (strcmp(fieldName, "TouchTimestamp") == 0)          inputReport.TouchData.Timestamp = (BYTE)value;
		else
		{
			wprintf(L"Unknown DualSense field: %hs\n", fieldName);
			continue;
		}

		wprintf(L"Sending report in 3 seconds...\n");
		Sleep(3000);
		HRESULT hr = sendReport(controller, &inputReport);
		wprintf(SUCCEEDED(hr) ? L"Report sent\n" : L"Send failed: 0x%08X\n", hr);
	}
}

/// <summary>
/// Runs the DualShock 4 input report loop.
/// </summary>
static void RunDs4Loop(void* controller, DuoController_SendReport_t sendReport)
{
	DUO_CONTROLLER_INPUT_REPORT_DS4 inputReport;
	memset(&inputReport, 0, sizeof(inputReport));
	char lineBuffer[128];

	while (fgets(lineBuffer, sizeof(lineBuffer), stdin) != NULL)
	{
		if (strcmp(lineBuffer, "exit\n") == 0)
			break;

		if (strcmp(lineBuffer, "reset\n") == 0)
		{
			memset(&inputReport, 0, sizeof(inputReport));
			HRESULT hr = sendReport(controller, &inputReport);
			wprintf(SUCCEEDED(hr) ? L"Report sent\n" : L"Send failed: 0x%08X\n", hr);
			continue;
		}

		char fieldName[64];
		int value = 0;
		if (sscanf_s(lineBuffer, "%63s %d", fieldName, (unsigned)_countof(fieldName), &value) != 2)
		{
			wprintf(L"Usage: fieldname value\n");
			continue;
		}

		if (strcmp(fieldName, "LeftStickHorizontal") == 0)      inputReport.LeftStickHorizontal = (BYTE)value;
		else if (strcmp(fieldName, "LeftStickVertical") == 0) inputReport.LeftStickVertical = (BYTE)value;
		else if (strcmp(fieldName, "RightStickHorizontal") == 0) inputReport.RightStickHorizontal = (BYTE)value;
		else if (strcmp(fieldName, "RightStickVertical") == 0) inputReport.RightStickVertical = (BYTE)value;
		else if (strcmp(fieldName, "DPad") == 0)        inputReport.DPad = (BYTE)value;
		else if (strcmp(fieldName, "Square") == 0)  inputReport.Square = (BYTE)value;
		else if (strcmp(fieldName, "Cross") == 0)   inputReport.Cross = (BYTE)value;
		else if (strcmp(fieldName, "Circle") == 0)  inputReport.Circle = (BYTE)value;
		else if (strcmp(fieldName, "Triangle") == 0) inputReport.Triangle = (BYTE)value;
		else if (strcmp(fieldName, "L1") == 0)      inputReport.L1 = (BYTE)value;
		else if (strcmp(fieldName, "R1") == 0)      inputReport.R1 = (BYTE)value;
		else if (strcmp(fieldName, "L2") == 0)      inputReport.L2 = (BYTE)value;
		else if (strcmp(fieldName, "R2") == 0)      inputReport.R2 = (BYTE)value;
		else if (strcmp(fieldName, "Share") == 0)   inputReport.Share = (BYTE)value;
		else if (strcmp(fieldName, "Options") == 0) inputReport.Options = (BYTE)value;
		else if (strcmp(fieldName, "L3") == 0)      inputReport.L3 = (BYTE)value;
		else if (strcmp(fieldName, "R3") == 0)      inputReport.R3 = (BYTE)value;
		else if (strcmp(fieldName, "PS") == 0)      inputReport.PS = (BYTE)value;
		else if (strcmp(fieldName, "Touchpad") == 0) inputReport.Touchpad = (BYTE)value;
		else if (strcmp(fieldName, "LeftTrigger") == 0)   inputReport.LeftTrigger = (BYTE)value;
		else if (strcmp(fieldName, "RightTrigger") == 0)  inputReport.RightTrigger = (BYTE)value;
		else if (strcmp(fieldName, "AngularVelocityX") == 0) inputReport.AngularVelocityX = (INT16)value;
		else if (strcmp(fieldName, "AngularVelocityY") == 0) inputReport.AngularVelocityY = (INT16)value;
		else if (strcmp(fieldName, "AngularVelocityZ") == 0) inputReport.AngularVelocityZ = (INT16)value;
		else if (strcmp(fieldName, "AccelerometerX") == 0)  inputReport.AccelerometerX = (INT16)value;
		else if (strcmp(fieldName, "AccelerometerY") == 0)  inputReport.AccelerometerY = (INT16)value;
		else if (strcmp(fieldName, "AccelerometerZ") == 0)  inputReport.AccelerometerZ = (INT16)value;
		else if (strcmp(fieldName, "TouchFinger1Index") == 0)       inputReport.TouchData.Finger[0].Index = (UINT32)value;
		else if (strcmp(fieldName, "TouchFinger1NotTouching") == 0) inputReport.TouchData.Finger[0].NotTouching = (UINT32)value;
		else if (strcmp(fieldName, "TouchFinger1X") == 0)           inputReport.TouchData.Finger[0].FingerX = (UINT32)value;
		else if (strcmp(fieldName, "TouchFinger1Y") == 0)           inputReport.TouchData.Finger[0].FingerY = (UINT32)value;
		else if (strcmp(fieldName, "TouchFinger2Index") == 0)       inputReport.TouchData.Finger[1].Index = (UINT32)value;
		else if (strcmp(fieldName, "TouchFinger2NotTouching") == 0) inputReport.TouchData.Finger[1].NotTouching = (UINT32)value;
		else if (strcmp(fieldName, "TouchFinger2X") == 0)           inputReport.TouchData.Finger[1].FingerX = (UINT32)value;
		else if (strcmp(fieldName, "TouchFinger2Y") == 0)           inputReport.TouchData.Finger[1].FingerY = (UINT32)value;
		else if (strcmp(fieldName, "TouchTimestamp") == 0)          inputReport.TouchData.Timestamp = (BYTE)value;
		else
		{
			wprintf(L"Unknown DualShock 4 field: %hs\n", fieldName);
			continue;
		}

		wprintf(L"Sending report in 3 seconds...\n");
		Sleep(3000);
		HRESULT hr = sendReport(controller, &inputReport);
		wprintf(SUCCEEDED(hr) ? L"Report sent\n" : L"Send failed: 0x%08X\n", hr);
	}
}

/// <summary>
/// The sample entry point.
/// </summary>
int main(int argc, char* argv[])
{
	HRESULT result = S_OK;

	wprintf(L"DuoController Sample Application\n");
	wprintf(L"Select controller type: 0 = Xbox, 1 = DualShock 4, 2 = DualSense, 3 = DualSense Edge\n");
	wprintf(L"Choice: ");

	char choiceBuffer[16];
	int choice = 0;
	if (fgets(choiceBuffer, sizeof(choiceBuffer), stdin) != NULL)
	{
		choice = atoi(choiceBuffer);
	}

	HMODULE duoController = LoadLibraryW(L"DuoController\\DuoController.dll");
	if (duoController == NULL)
	{
		wprintf(L"Failed to load DuoController.dll: 0x%08X\n", HRESULT_FROM_WIN32(GetLastError()));
		return (int)HRESULT_FROM_WIN32(GetLastError());
	}

	DuoController_Initialize_t DuoController_Initialize_Dynamic = (DuoController_Initialize_t)GetProcAddress(duoController, "DuoController_Initialize");
	DuoController_Uninitialize_t DuoController_Uninitialize_Dynamic = (DuoController_Uninitialize_t)GetProcAddress(duoController, "DuoController_Uninitialize");
	DuoController_CreateController_t DuoController_CreateController_Dynamic = (DuoController_CreateController_t)GetProcAddress(duoController, "DuoController_CreateController");
	DuoController_RemoveController_t DuoController_RemoveController_Dynamic = (DuoController_RemoveController_t)GetProcAddress(duoController, "DuoController_RemoveController");
	DuoController_SendReport_t DuoController_SendReport_Dynamic = (DuoController_SendReport_t)GetProcAddress(duoController, "DuoController_SendReport");

	if (DuoController_Initialize_Dynamic == NULL ||
		DuoController_Uninitialize_Dynamic == NULL ||
		DuoController_CreateController_Dynamic == NULL ||
		DuoController_RemoveController_Dynamic == NULL ||
		DuoController_SendReport_Dynamic == NULL)
	{
		wprintf(L"Failed to resolve required DuoController functions\n");
		FreeLibrary(duoController);
		return E_FAIL;
	}

	if (FAILED(result = DuoController_Initialize_Dynamic()))
	{
		wprintf(L"Failed to initialize DuoController library: 0x%08X\n", result);
		FreeLibrary(duoController);
		return result;
	}

	DUO_CONTROLLER_TYPE controllerType = choice < 0 || choice > 3 ? DuoControllerTypeXbox : (DUO_CONTROLLER_TYPE)choice;

	void* controller = NULL;
	result = DuoController_CreateController_Dynamic(
		controllerType,
		VibrationReportCallback, NULL, &controller);

	if (FAILED(result))
	{
		wprintf(L"Failed to create Duo controller: 0x%08X\n", result);
		DuoController_Uninitialize_Dynamic();
		FreeLibrary(duoController);
		return result;
	}

	switch (controllerType)
	{
	case DuoControllerTypeDualShock4: RunDs4Loop(controller, DuoController_SendReport_Dynamic); break;
	case DuoControllerTypeDualSense:
	case DuoControllerTypeDualSenseEdge: RunDualSenseLoop(controller, DuoController_SendReport_Dynamic); break;
	default: RunXboxLoop(controller, DuoController_SendReport_Dynamic); break;
	}

	DuoController_RemoveController_Dynamic(controller);
	DuoController_Uninitialize_Dynamic();
	FreeLibrary(duoController);

	return S_OK;
}
