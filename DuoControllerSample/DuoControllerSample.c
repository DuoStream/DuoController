#include "../DuoController/DuoController.h"
#include <stdio.h>
#include <stdlib.h>

typedef HRESULT (*DuoController_Initialize_t)();
typedef HRESULT (*DuoController_Uninitialize_t)();
typedef HRESULT(*DuoController_CreateController_t)(DUO_CONTROLLER_TYPE controllerType, DuoController_VibrationReportCallback_t vibrationCallback, void* vibrationCallbackContext, void** controller);
typedef HRESULT(*DuoController_RemoveController_t)(void* controller);
typedef HRESULT(*DuoController_SendReport_t)(void* controller, DUO_CONTROLLER_INPUT_REPORT* inputReport);
typedef HRESULT(*DuoController_SendReportDs_t)(void* controller, DUO_CONTROLLER_INPUT_REPORT_DS* inputReport);

typedef enum _SAMPLE_CONTROLLER_TYPE
{
	SampleControllerTypeXbox,
	SampleControllerTypeDs
} SAMPLE_CONTROLLER_TYPE;

/// <summary>
/// Receives vibration reports from the Duo controller.
/// </summary>
/// <param name="controller">The controller</param>
/// <param name="smallMotorSpeed">The speed of the small motor</param>
/// <param name="largeMotorSpeed">The speed of the large motor</param>
/// <param name="context">The context</param>
void VibrationReportCallback(void* controller, DUO_CONTROLLER_FORCE_FEEDBACK_REPORT* report, void* context)
{
	// Log the vibration report
	wprintf(L"Vibration report received: LeftMotor=%d, RightMotor=%d, Duration=%d, Delay=%d, Repeat=%d\n", report->LeftMotor, report->RightMotor, report->Duration, report->Delay, report->Repeat);
}

/// <summary>
/// Runs the Xbox input report loop.
/// </summary>
static void RunXboxLoop(void* controller, DuoController_SendReport_t sendReport)
{
	DUO_CONTROLLER_INPUT_REPORT inputReport;
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
		else
		{
			wprintf(L"Unknown Xbox field: %s\n", buttonName);
			continue;
		}

		wprintf(L"Sending report in 3 seconds...\n");
		Sleep(3000);
		HRESULT hr = sendReport(controller, &inputReport);
		wprintf(SUCCEEDED(hr) ? L"Report sent\n" : L"Send failed: 0x%08X\n", hr);
	}
}

/// <summary>
/// Runs the DualSense input report loop.
/// </summary>
static void RunDsLoop(void* controller, DuoController_SendReportDs_t sendReportDs)
{
	DUO_CONTROLLER_INPUT_REPORT_DS inputReport;
	memset(&inputReport, 0, sizeof(inputReport));
	char lineBuffer[128];

	while (fgets(lineBuffer, sizeof(lineBuffer), stdin) != NULL)
	{
		if (strcmp(lineBuffer, "exit\n") == 0)
			break;

		if (strcmp(lineBuffer, "reset\n") == 0)
		{
			memset(&inputReport, 0, sizeof(inputReport));
			HRESULT hr = sendReportDs(controller, &inputReport);
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

		if (strcmp(fieldName, "LeftStickX") == 0)      inputReport.LeftStickX = (BYTE)value;
		else if (strcmp(fieldName, "LeftStickY") == 0) inputReport.LeftStickY = (BYTE)value;
		else if (strcmp(fieldName, "RightStickX") == 0) inputReport.RightStickX = (BYTE)value;
		else if (strcmp(fieldName, "RightStickY") == 0) inputReport.RightStickY = (BYTE)value;
		else if (strcmp(fieldName, "DPad") == 0)        inputReport.DPad = (BYTE)value;
		else if (strcmp(fieldName, "ButtonSquare") == 0)  inputReport.ButtonSquare = (BYTE)value;
		else if (strcmp(fieldName, "ButtonCross") == 0)   inputReport.ButtonCross = (BYTE)value;
		else if (strcmp(fieldName, "ButtonCircle") == 0)  inputReport.ButtonCircle = (BYTE)value;
		else if (strcmp(fieldName, "ButtonTriangle") == 0) inputReport.ButtonTriangle = (BYTE)value;
		else if (strcmp(fieldName, "ButtonL1") == 0)      inputReport.ButtonL1 = (BYTE)value;
		else if (strcmp(fieldName, "ButtonR1") == 0)      inputReport.ButtonR1 = (BYTE)value;
		else if (strcmp(fieldName, "ButtonL2") == 0)      inputReport.ButtonL2 = (BYTE)value;
		else if (strcmp(fieldName, "ButtonR2") == 0)      inputReport.ButtonR2 = (BYTE)value;
		else if (strcmp(fieldName, "ButtonCreate") == 0)  inputReport.ButtonCreate = (BYTE)value;
		else if (strcmp(fieldName, "ButtonOptions") == 0) inputReport.ButtonOptions = (BYTE)value;
		else if (strcmp(fieldName, "ButtonL3") == 0)      inputReport.ButtonL3 = (BYTE)value;
		else if (strcmp(fieldName, "ButtonR3") == 0)      inputReport.ButtonR3 = (BYTE)value;
		else if (strcmp(fieldName, "ButtonHome") == 0)    inputReport.ButtonHome = (BYTE)value;
		else if (strcmp(fieldName, "ButtonPad") == 0)     inputReport.ButtonPad = (BYTE)value;
		else if (strcmp(fieldName, "ButtonMute") == 0)    inputReport.ButtonMute = (BYTE)value;
		else if (strcmp(fieldName, "ButtonLeftFunction") == 0)  inputReport.ButtonLeftFunction = (BYTE)value;
		else if (strcmp(fieldName, "ButtonRightFunction") == 0) inputReport.ButtonRightFunction = (BYTE)value;
		else if (strcmp(fieldName, "ButtonLeftPaddle") == 0)    inputReport.ButtonLeftPaddle = (BYTE)value;
		else if (strcmp(fieldName, "ButtonRightPaddle") == 0)   inputReport.ButtonRightPaddle = (BYTE)value;
		else if (strcmp(fieldName, "TriggerLeft") == 0)   inputReport.TriggerLeft = (BYTE)value;
		else if (strcmp(fieldName, "TriggerRight") == 0)  inputReport.TriggerRight = (BYTE)value;
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
			wprintf(L"Unknown DualSense field: %s\n", fieldName);
			continue;
		}

		wprintf(L"Sending report in 3 seconds...\n");
		Sleep(3000);
		HRESULT hr = sendReportDs(controller, &inputReport);
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
	wprintf(L"Select controller type: 1 = Xbox, 2 = DualSense\n");
	wprintf(L"Choice: ");

	char choiceBuffer[16];
	SAMPLE_CONTROLLER_TYPE sampleType = SampleControllerTypeXbox;
	if (fgets(choiceBuffer, sizeof(choiceBuffer), stdin) != NULL)
	{
		int choice = atoi(choiceBuffer);
		if (choice == 2)
			sampleType = SampleControllerTypeDs;
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
	DuoController_SendReportDs_t DuoController_SendReportDs_Dynamic = (DuoController_SendReportDs_t)GetProcAddress(duoController, "DuoController_SendReportDs");

	if (DuoController_Initialize_Dynamic == NULL ||
		DuoController_Uninitialize_Dynamic == NULL ||
		DuoController_CreateController_Dynamic == NULL ||
		DuoController_RemoveController_Dynamic == NULL ||
		(sampleType == SampleControllerTypeXbox && DuoController_SendReport_Dynamic == NULL) ||
		(sampleType == SampleControllerTypeDs && DuoController_SendReportDs_Dynamic == NULL))
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

	void* controller = NULL;
	result = DuoController_CreateController_Dynamic(
		sampleType == SampleControllerTypeDs ? DuoControllerTypeDs : DuoControllerTypeXbox,
		VibrationReportCallback, NULL, &controller);

	if (FAILED(result))
	{
		wprintf(L"Failed to create Duo controller: 0x%08X\n", result);
		DuoController_Uninitialize_Dynamic();
		FreeLibrary(duoController);
		return result;
	}

	if (sampleType == SampleControllerTypeDs)
		RunDsLoop(controller, DuoController_SendReportDs_Dynamic);
	else
		RunXboxLoop(controller, DuoController_SendReport_Dynamic);

	DuoController_RemoveController_Dynamic(controller);
	DuoController_Uninitialize_Dynamic();
	FreeLibrary(duoController);

	return S_OK;
}
