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

#include "DuoController.h"
#include <winstring.h>
#include <initguid.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <newdev.h>
#include <stdio.h>
#include "Public.h"

#pragma comment(lib, "mincore.lib")
#pragma comment(lib, "onecore.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

#ifndef STATUS_WAIT_0
#define STATUS_WAIT_0 ((DWORD)0x00000000L)
#endif

#pragma pack(push, 1)

/// <summary>
/// The DuoController structure.
/// </summary>
typedef struct _DUO_CONTROLLER
{
	DUO_CONTROLLER_TYPE Type;

	DuoController_VibrationReportCallback_t VibrationReportCallback;
	void* VibrationReportCallbackContext;

	// Xbox-specific fields (XInputHID via shared memory HID device)
	WCHAR XboxInstanceId[256];
	WCHAR XboxHidInstanceId[256];
	HANDLE XboxInputMapping;
	HANDLE XboxOutputMapping;
	LPVOID XboxInputView;
	LPVOID XboxOutputView;
	HANDLE XboxInputEvent;
	HANDLE XboxOutputEvent;
	HANDLE XboxFfbThread;
	HANDLE XboxFfbStopEvent;
	CRITICAL_SECTION XboxCs;
	DUO_CONTROLLER_INPUT_REPORT_XBOX LastXboxInputReport;
#ifdef _DEBUG
	HANDLE XboxDebugMapping;
	LPVOID XboxDebugView;
	HANDLE XboxDebugThread;
	HANDLE XboxDebugStopEvent;
#endif

	// DualSense Edge-specific fields
	WCHAR DsInstanceId[256];
	WCHAR DsHidInstanceId[256];
	HANDLE DsInputMapping;
	HANDLE DsOutputMapping;
	LPVOID DsInputView;
	LPVOID DsOutputView;
	HANDLE DsInputEvent;
	HANDLE DsOutputEvent;
	HANDLE DsFfbThread;
	HANDLE DsFfbStopEvent;
	CRITICAL_SECTION DsCs;
	DUO_CONTROLLER_INPUT_REPORT_DUALSENSE LastDsInputReport;

	// DualShock 4-specific fields
	WCHAR Ds4InstanceId[256];
	WCHAR Ds4HidInstanceId[256];
	HANDLE Ds4InputMapping;
	HANDLE Ds4OutputMapping;
	LPVOID Ds4InputView;
	LPVOID Ds4OutputView;
	HANDLE Ds4InputEvent;
	HANDLE Ds4OutputEvent;
	HANDLE Ds4FfbThread;
	HANDLE Ds4FfbStopEvent;
	CRITICAL_SECTION Ds4Cs;
	DUO_CONTROLLER_INPUT_REPORT_DS4 LastDs4InputReport;
} DUO_CONTROLLER;

#pragma pack(pop)

static DWORD SessionId;
static BOOL Initialized;
static DUO_CONTROLLER** Controllers = NULL;
static DWORD ControllerCount = 0;

static void InitializeWindowsRuntimeForCurrentThread()
{
	(void)RoInitialize(RO_INIT_MULTITHREADED);
}

static HRESULT DsWin32ErrorToHresult(DWORD error)
{
	switch (error)
	{
	case ERROR_SUCCESS:
		return S_OK;
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	case ERROR_ACCESS_DENIED:
		return E_ACCESSDENIED;
	case ERROR_INVALID_PARAMETER:
		return E_INVALIDARG;
	case ERROR_PIPE_NOT_CONNECTED:
	case ERROR_NO_DATA:
	case ERROR_BROKEN_PIPE:
		return HRESULT_FROM_WIN32(ERROR_PIPE_NOT_CONNECTED);
	case ERROR_OUTOFMEMORY:
		return E_OUTOFMEMORY;
	default:
		return HRESULT_FROM_WIN32(error);
	}
}

static void SanitizeInstanceIdForPipeName(const WCHAR* instanceId, WCHAR* sanitized, size_t sanitizedSize)
{
	size_t i;
	for (i = 0; i < sanitizedSize - 1 && instanceId[i] != L'\0'; i++)
	{
		sanitized[i] = (instanceId[i] == L'\\') ? L'_' : instanceId[i];
	}
	sanitized[i] = L'\0';
}

static BOOL IsProcessElevated()
{
	HANDLE token = NULL;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
		return FALSE;
	TOKEN_ELEVATION elevation;
	DWORD size = 0;
	BOOL elevated = FALSE;
	if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size))
		elevated = elevation.TokenIsElevated;
	CloseHandle(token);
	return elevated;
}

// Generates a fresh, unique per-creation token (32 lowercase hex chars) that is
// embedded in the device-ID seed. A new token on every creation guarantees the
// resulting device instance ID is never a reincarnation of a previously
// installed (and removed) controller, so running applications see a genuine
// new-device arrival for each pad instead of a reused instance identity.
// The token combines the system tick count, process ID, and a monotonic
// counter, so it cannot repeat within a boot and only in a negligible case
// across reboots.
static void GenerateUniqueDeviceToken(WCHAR* token, DWORD tokenSize)
{
	static volatile LONG tokenCounter;
	ULONGLONG tick = GetTickCount64();
	swprintf_s(token, tokenSize,
		L"%08x%08x%08x%08x",
		(DWORD)tick, (DWORD)(tick >> 32),
		GetCurrentProcessId(),
		(DWORD)InterlockedIncrement(&tokenCounter));
}

static HRESULT RemoveDuoControllerDevice(const WCHAR* instanceId);
static HRESULT RemoveDualSenseController(DUO_CONTROLLER* controller);
static HRESULT RemoveDualShock4Controller(DUO_CONTROLLER* controller);
static HRESULT RemoveXboxController(DUO_CONTROLLER* controller);

static HRESULT InstallDuoControllerDevice(const WCHAR* hardwareId, const WCHAR* deviceIdSeed, WCHAR* instanceId, DWORD instanceIdSize, WCHAR* hidInstanceId, DWORD hidInstanceIdSize)
{
	HRESULT result = S_OK;
	HMODULE hMod = NULL;
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)&DuoController_Initialize, &hMod))
		return HRESULT_FROM_WIN32(GetLastError());
	WCHAR dllPath[MAX_PATH];
	if (GetModuleFileNameW(hMod, dllPath, MAX_PATH) == 0)
		return HRESULT_FROM_WIN32(GetLastError());
	WCHAR* lastSlash = wcsrchr(dllPath, L'\\');
	if (lastSlash == NULL)
		return E_UNEXPECTED;
	*(lastSlash + 1) = L'\0';
	WCHAR infPath[MAX_PATH];
	wcscpy_s(infPath, MAX_PATH, dllPath);
	wcscat_s(infPath, MAX_PATH, L"DuoController.inf");
	if (GetFileAttributesW(infPath) == INVALID_FILE_ATTRIBUTES)
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	if (!IsProcessElevated())
		return E_ACCESSDENIED;
	WCHAR fullInfPath[MAX_PATH];
	if (!GetFullPathNameW(infPath, MAX_PATH, fullInfPath, NULL))
		return HRESULT_FROM_WIN32(GetLastError());
	GUID classGuid;
	WCHAR className[MAX_CLASS_NAME_LEN];
	DWORD requiredSize = 0;
	if (!SetupDiGetINFClassW(fullInfPath, &classGuid, className, MAX_CLASS_NAME_LEN, &requiredSize))
		return HRESULT_FROM_WIN32(GetLastError());
	HDEVINFO hDevInfo = SetupDiCreateDeviceInfoList(&classGuid, NULL);
	if (hDevInfo == INVALID_HANDLE_VALUE)
		return HRESULT_FROM_WIN32(GetLastError());
	BOOL rebootRequired = FALSE;
	SetupCopyOEMInfW(fullInfPath, NULL, SPOST_NONE, SP_COPY_NOOVERWRITE, NULL, 0, NULL, NULL);
	// Install the driver package without DIIRFLAG_FORCE_INF. That flag makes
	// DiInstallDriverW reinstall the driver onto every device matching the INF's
	// hardware IDs, which restarts existing pads whenever a new pad is created
	// (the multi-controller disconnect/reconnect storm). Without it, an already
	// imported package of equal version updates no device; DiInstallDriverW then
	// returns ERROR_NO_MORE_ITEMS, which is expected here and must not be
	// treated as a failure.
	if (!DiInstallDriverW(NULL, fullInfPath, 0, &rebootRequired))
	{
		DWORD dwError = GetLastError();
		if (dwError != ERROR_NO_MORE_ITEMS)
		{
			result = HRESULT_FROM_WIN32(dwError);
			SetupDiDestroyDeviceInfoList(hDevInfo);
			return result;
		}
	}
	SP_DEVINFO_DATA devInfoData;
	ZeroMemory(&devInfoData, sizeof(devInfoData));
	devInfoData.cbSize = sizeof(devInfoData);
	if (!SetupDiCreateDeviceInfoW(hDevInfo, deviceIdSeed, &classGuid, NULL, NULL, DICD_GENERATE_ID, &devInfoData))
	{
		result = HRESULT_FROM_WIN32(GetLastError());
		SetupDiDestroyDeviceInfoList(hDevInfo);
		return result;
	}
	DWORD hwIdLen = (DWORD)(wcslen(hardwareId) * sizeof(WCHAR));
	if (!SetupDiSetDeviceRegistryPropertyW(hDevInfo, &devInfoData, SPDRP_HARDWAREID, (const BYTE*)hardwareId, hwIdLen + sizeof(WCHAR)))
	{
		result = HRESULT_FROM_WIN32(GetLastError());
		SetupDiDestroyDeviceInfoList(hDevInfo);
		return result;
	}
	if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, hDevInfo, &devInfoData))
	{
		result = HRESULT_FROM_WIN32(GetLastError());
		SetupDiDestroyDeviceInfoList(hDevInfo);
		return result;
	}
	if (!SetupDiGetDeviceInstanceIdW(hDevInfo, &devInfoData, instanceId, instanceIdSize, NULL))
	{
		result = HRESULT_FROM_WIN32(GetLastError());
		SetupDiDestroyDeviceInfoList(hDevInfo);
		return result;
	}
	HKEY duoRegistryKey;
	LONG status = RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Duo", 0, NULL, REG_OPTION_VOLATILE, KEY_READ | KEY_WRITE, NULL, &duoRegistryKey, NULL);
	if (status == ERROR_SUCCESS)
	{
		LPCWSTR valueName = L"DuoControllerSessionId";
		RegSetValueExW(duoRegistryKey, valueName, 0, REG_DWORD, (const BYTE*)&SessionId, sizeof(SessionId));
		if (!DiInstallDevice(NULL, hDevInfo, &devInfoData, NULL, DIIDFLAG_INSTALLCOPYINFDRIVERS, &rebootRequired))
		{
			result = HRESULT_FROM_WIN32(GetLastError());
			// The devnode was already registered by DIF_REGISTERDEVICE above, so a
			// failed install leaves a not-started ghost device behind. Remove it so
			// repeated failed creates do not accumulate stale pads.
			RemoveDuoControllerDevice(instanceId);
			SetupDiDestroyDeviceInfoList(hDevInfo);
			return result;
		}
		if (hidInstanceId != NULL && hidInstanceIdSize > 0)
		{
			// The mshidumdf HID child ID is recorded nowhere that it is later
			// consumed: removal removes the parent devnode and the child is torn
			// down by PnP as part of the cascade. Polling for the child here used
			// to block every create for the full poll window while the child lagged
			// behind the parent's start, which stalled the creating application's
			// input thread (apps such as Sunshine create pads synchronously from
			// their input loop). The objects the connect phase actually needs are
			// the shared-memory mappings created by the driver during device add,
			// and those are polled for directly by the Ds/Ds4/XboxConnect* helpers
			// below. Leave the ID empty instead of blocking.
			hidInstanceId[0] = L'\0';
		}
		RegDeleteValueW(duoRegistryKey, valueName);
		RegCloseKey(duoRegistryKey);
	}
	SetupDiDestroyDeviceInfoList(hDevInfo);
	return result;
}

static HRESULT RemoveDuoControllerDevice(const WCHAR* instanceId)
{
	if (!IsProcessElevated())
		return E_ACCESSDENIED;

	// Remove the parent device first. Its children (the HID PDO created by
	// mshidumdf.sys) are removed as part of the cascade, so the HID child must
	// not be removed on its own first. Removing the child first triggered a
	// HID-class re-enumeration that briefly disconnected every other pad right
	// when a new pad was being added, causing applications to drop the new
	// pad's arrival notification (detection is notification-driven).
	HDEVINFO hDevInfo = SetupDiCreateDeviceInfoList(NULL, NULL);
	if (hDevInfo == INVALID_HANDLE_VALUE)
		return HRESULT_FROM_WIN32(GetLastError());
	HRESULT result = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	SP_DEVINFO_DATA devInfoData;
	devInfoData.cbSize = sizeof(devInfoData);
	if (SetupDiOpenDeviceInfoW(hDevInfo, instanceId, NULL, 0, &devInfoData))
	{
		SP_REMOVEDEVICE_PARAMS removeParams;
		ZeroMemory(&removeParams, sizeof(removeParams));
		removeParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
		removeParams.ClassInstallHeader.InstallFunction = DIF_REMOVE;
		removeParams.Scope = DI_REMOVEDEVICE_GLOBAL;
		if (SetupDiSetClassInstallParamsW(hDevInfo, &devInfoData,
			(PSP_CLASSINSTALL_HEADER)&removeParams, sizeof(removeParams)))
		{
			if (SetupDiCallClassInstaller(DIF_REMOVE, hDevInfo, &devInfoData) ||
				SetupDiRemoveDevice(hDevInfo, &devInfoData))
			{
				result = S_OK;
			}
			else
			{
				result = HRESULT_FROM_WIN32(GetLastError());
			}
		}
		else
		{
			result = HRESULT_FROM_WIN32(GetLastError());
		}
	}
	SetupDiDestroyDeviceInfoList(hDevInfo);

	// Wait for the devnode to actually disappear. PnP removes the device
	// asynchronously; re-creating with the same seed before removal completes
	// makes Windows assign a new instance ID and can leave a stale/ghost node,
	// which in turn makes applications miss the next connect.
	if (SUCCEEDED(result))
	{
		ULONGLONG removeStart = GetTickCount64();
		while ((GetTickCount64() - removeStart) < 10000)
		{
			WCHAR mutableId[256];
			wcscpy_s(mutableId, ARRAYSIZE(mutableId), instanceId);
			DEVINST devInst;
			if (CM_Locate_DevNodeW(&devInst, mutableId, CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS)
				break;
			Sleep(100);
		}
	}

	return result;
}

// ==================== DualSense Edge shared memory helpers ====================

static HRESULT DsConnectInput(DUO_CONTROLLER* controller)
{
	WCHAR sanitized[256];
	SanitizeInstanceIdForPipeName(controller->DsInstanceId, sanitized, ARRAYSIZE(sanitized));
	WCHAR mappingName[512];
	swprintf_s(mappingName, ARRAYSIZE(mappingName), L"Global\\Duo_%s_input", sanitized);
	WCHAR eventName[512];
	swprintf_s(eventName, ARRAYSIZE(eventName), L"Global\\Duo_%s_input_event", sanitized);
	ULONGLONG startTime = GetTickCount64();
	HANDLE hMapping = NULL;
	while (GetTickCount64() - startTime < 1000)
	{
		hMapping = OpenFileMappingW(FILE_MAP_WRITE, FALSE, mappingName);
		if (hMapping != NULL)
			break;
		Sleep(10);
	}
	if (hMapping == NULL)
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	LPVOID view = MapViewOfFile(hMapping, FILE_MAP_WRITE, 0, 0, 0);
	if (view == NULL)
	{
		CloseHandle(hMapping);
		return DsWin32ErrorToHresult(GetLastError());
	}
	HANDLE hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, eventName);
	if (hEvent == NULL)
	{
		UnmapViewOfFile(view);
		CloseHandle(hMapping);
		return DsWin32ErrorToHresult(GetLastError());
	}
	controller->DsInputMapping = hMapping;
	controller->DsInputView = view;
	controller->DsInputEvent = hEvent;
	return S_OK;
}

static DWORD WINAPI DsFfbThreadProc(LPVOID param)
{
	DUO_CONTROLLER* controller = (DUO_CONTROLLER*)param;
	HANDLE waitHandles[2] = { controller->DsFfbStopEvent, controller->DsOutputEvent };
	while (1)
	{
		DWORD wr = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
		if (wr == WAIT_OBJECT_0)
			break;
		if (wr == WAIT_OBJECT_0 + 1)
		{
			DUO_CONTROLLER_OUTPUT_REPORT_DS* outputMem = (DUO_CONTROLLER_OUTPUT_REPORT_DS*)controller->DsOutputView;
			if (outputMem->ReportId == DS_OUTPUT_REPORT_ID)
			{
				DUO_CONTROLLER_FORCE_FEEDBACK_REPORT ffReport;
				ZeroMemory(&ffReport, sizeof(ffReport));
				ffReport.Flags = SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_RIGHT_MOTOR_VALID | SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_LEFT_MOTOR_VALID;
				if (outputMem->UseRumbleNotHaptics)
				{
					ffReport.RightMotor = outputMem->RumbleEmulationRight;
					ffReport.LeftMotor = outputMem->RumbleEmulationLeft;
				}
#pragma warning(suppress:4366)
				EnterCriticalSection(&controller->DsCs);
				DuoController_VibrationReportCallback_t callback = controller->VibrationReportCallback;
				void* context = controller->VibrationReportCallbackContext;
#pragma warning(suppress:4366)
				LeaveCriticalSection(&controller->DsCs);
				if (callback != NULL)
					callback(controller, &ffReport, context);
			}
		}
	}
	return 0;
}

static HRESULT DsConnectFfb(DUO_CONTROLLER* controller)
{
	WCHAR sanitized[256];
	SanitizeInstanceIdForPipeName(controller->DsInstanceId, sanitized, ARRAYSIZE(sanitized));
	WCHAR mappingName[512];
	swprintf_s(mappingName, ARRAYSIZE(mappingName), L"Global\\Duo_%s_output", sanitized);
	WCHAR eventName[512];
	swprintf_s(eventName, ARRAYSIZE(eventName), L"Global\\Duo_%s_output_event", sanitized);
	ULONGLONG startTime = GetTickCount64();
	HANDLE hMapping = NULL;
	while (GetTickCount64() - startTime < 1000)
	{
		hMapping = OpenFileMappingW(FILE_MAP_READ, FALSE, mappingName);
		if (hMapping != NULL)
			break;
		Sleep(10);
	}
	if (hMapping == NULL)
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	LPVOID view = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
	if (view == NULL)
	{
		CloseHandle(hMapping);
		return DsWin32ErrorToHresult(GetLastError());
	}
	HANDLE hEvent = OpenEventW(SYNCHRONIZE, FALSE, eventName);
	if (hEvent == NULL)
	{
		UnmapViewOfFile(view);
		CloseHandle(hMapping);
		return DsWin32ErrorToHresult(GetLastError());
	}
	controller->DsOutputMapping = hMapping;
	controller->DsOutputView = view;
	controller->DsOutputEvent = hEvent;
	controller->DsFfbStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!controller->DsFfbStopEvent)
	{
		CloseHandle(hEvent);
		UnmapViewOfFile(view);
		CloseHandle(hMapping);
		controller->DsOutputEvent = NULL;
		controller->DsOutputView = NULL;
		controller->DsOutputMapping = NULL;
		return HRESULT_FROM_WIN32(GetLastError());
	}
	controller->DsFfbThread = CreateThread(NULL, 0, DsFfbThreadProc, controller, 0, NULL);
	if (!controller->DsFfbThread)
	{
		CloseHandle(controller->DsFfbStopEvent);
		controller->DsFfbStopEvent = NULL;
		CloseHandle(hEvent);
		controller->DsOutputEvent = NULL;
		UnmapViewOfFile(view);
		controller->DsOutputView = NULL;
		CloseHandle(hMapping);
		controller->DsOutputMapping = NULL;
		return HRESULT_FROM_WIN32(GetLastError());
	}
	return S_OK;
}

static void DsDisconnectInput(DUO_CONTROLLER* controller)
{
	if (controller->DsInputEvent != NULL)
	{
		CloseHandle(controller->DsInputEvent);
		controller->DsInputEvent = NULL;
	}
	if (controller->DsInputView != NULL)
	{
		UnmapViewOfFile(controller->DsInputView);
		controller->DsInputView = NULL;
	}
	if (controller->DsInputMapping != NULL)
	{
		CloseHandle(controller->DsInputMapping);
		controller->DsInputMapping = NULL;
	}
}

static void DsDisconnectFfb(DUO_CONTROLLER* controller)
{
	if (controller->DsFfbStopEvent != NULL)
	{
		SetEvent(controller->DsFfbStopEvent);
		if (controller->DsFfbThread != NULL)
		{
			WaitForSingleObject(controller->DsFfbThread, 1000);
			CloseHandle(controller->DsFfbThread);
			controller->DsFfbThread = NULL;
		}
		CloseHandle(controller->DsFfbStopEvent);
		controller->DsFfbStopEvent = NULL;
	}
	if (controller->DsOutputEvent != NULL)
	{
		CloseHandle(controller->DsOutputEvent);
		controller->DsOutputEvent = NULL;
	}
	if (controller->DsOutputView != NULL)
	{
		UnmapViewOfFile(controller->DsOutputView);
		controller->DsOutputView = NULL;
	}
	if (controller->DsOutputMapping != NULL)
	{
		CloseHandle(controller->DsOutputMapping);
		controller->DsOutputMapping = NULL;
	}
}

static HRESULT DsSendRawInput(DUO_CONTROLLER* controller, const DUO_CONTROLLER_INPUT_REPORT_DUALSENSE* state)
{
	if (controller->DsInputView == NULL)
		return HRESULT_FROM_WIN32(ERROR_PIPE_NOT_CONNECTED);
	BYTE report[DS_REPORT_SIZE];
	ZeroMemory(report, DS_REPORT_SIZE);
	report[0] = DS_INPUT_REPORT_ID;
	memcpy(&report[1], state, sizeof(DUO_CONTROLLER_INPUT_REPORT_DUALSENSE));
	if (state->PowerPercent == 0 && state->PowerState == 0)
		report[53] = (0x00 << 4) | 0x0A;
	if (state->PluggedUsbData == 0 && state->PluggedUsbPower == 0)
		report[54] = 0x18;
	BYTE* inputMem = (BYTE*)controller->DsInputView;
	inputMem[0] = INPUT_REPORT_FULL;
	inputMem[1] = DS_REPORT_SIZE;
	memcpy(&inputMem[MESSAGE_HEADER_LEN], report, DS_REPORT_SIZE);
	if (!SetEvent(controller->DsInputEvent))
		return DsWin32ErrorToHresult(GetLastError());
	return S_OK;
}

static HRESULT CreateDualSenseController(DUO_CONTROLLER* controller, USHORT pid)
{
	WCHAR instanceId[256];
	WCHAR hidInstanceId[256];
	WCHAR hwid[64];
	swprintf_s(hwid, ARRAYSIZE(hwid), L"Root\\VID_054C&PID_%04X", pid);
	WCHAR token[40];
	GenerateUniqueDeviceToken(token, ARRAYSIZE(token));
	WCHAR seed[96];
	swprintf_s(seed, ARRAYSIZE(seed), L"VID_054C&PID_%04X&DUOCONTROLLER&%s", pid, token);
	HRESULT result = InstallDuoControllerDevice(hwid, seed, instanceId, ARRAYSIZE(instanceId), hidInstanceId, ARRAYSIZE(hidInstanceId));
	if (FAILED(result))
		return result;
	wcscpy_s(controller->DsInstanceId, ARRAYSIZE(controller->DsInstanceId), instanceId);
	wcscpy_s(controller->DsHidInstanceId, ARRAYSIZE(controller->DsHidInstanceId), hidInstanceId);
	controller->DsInputMapping = NULL;
	controller->DsOutputMapping = NULL;
	controller->DsInputView = NULL;
	controller->DsOutputView = NULL;
	controller->DsInputEvent = NULL;
	controller->DsOutputEvent = NULL;
	controller->DsFfbThread = NULL;
	controller->DsFfbStopEvent = NULL;
#pragma warning(suppress:4366)
	InitializeCriticalSection(&controller->DsCs);
	result = DsConnectInput(controller);
	if (FAILED(result))
	{
		RemoveDualSenseController(controller);
		return result;
	}
	result = DsConnectFfb(controller);
	if (FAILED(result))
	{
		RemoveDualSenseController(controller);
		return result;
	}
	return S_OK;
}

static HRESULT RemoveDualSenseController(DUO_CONTROLLER* controller)
{
	DsDisconnectFfb(controller);
	DsDisconnectInput(controller);
#pragma warning(suppress:4366)
	DeleteCriticalSection(&controller->DsCs);
	return RemoveDuoControllerDevice(controller->DsInstanceId);
}

static HRESULT SendDsReport(DUO_CONTROLLER* controller, DUO_CONTROLLER_INPUT_REPORT_DUALSENSE* inputReport)
{
#pragma warning(suppress:4366)
	EnterCriticalSection(&controller->DsCs);
	inputReport->SeqNo++;
	inputReport->DeviceTimeStamp = (UINT32)GetTickCount64();
	inputReport->SensorTimestamp = inputReport->DeviceTimeStamp;
	HRESULT result = DsSendRawInput(controller, inputReport);
	if (SUCCEEDED(result))
		controller->LastDsInputReport = *inputReport;
#pragma warning(suppress:4366)
	LeaveCriticalSection(&controller->DsCs);
	return result;
}

// ==================== DualShock 4 shared memory helpers ====================

static HRESULT Ds4ConnectInput(DUO_CONTROLLER* controller)
{
	WCHAR sanitized[256];
	SanitizeInstanceIdForPipeName(controller->Ds4InstanceId, sanitized, ARRAYSIZE(sanitized));
	WCHAR mappingName[512];
	swprintf_s(mappingName, ARRAYSIZE(mappingName), L"Global\\Duo_%s_input", sanitized);
	WCHAR eventName[512];
	swprintf_s(eventName, ARRAYSIZE(eventName), L"Global\\Duo_%s_input_event", sanitized);
	ULONGLONG startTime = GetTickCount64();
	HANDLE hMapping = NULL;
	while (GetTickCount64() - startTime < 1000)
	{
		hMapping = OpenFileMappingW(FILE_MAP_WRITE, FALSE, mappingName);
		if (hMapping != NULL)
			break;
		Sleep(10);
	}
	if (hMapping == NULL)
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	LPVOID view = MapViewOfFile(hMapping, FILE_MAP_WRITE, 0, 0, 0);
	if (view == NULL)
	{
		CloseHandle(hMapping);
		return DsWin32ErrorToHresult(GetLastError());
	}
	HANDLE hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, eventName);
	if (hEvent == NULL)
	{
		UnmapViewOfFile(view);
		CloseHandle(hMapping);
		return DsWin32ErrorToHresult(GetLastError());
	}
	controller->Ds4InputMapping = hMapping;
	controller->Ds4InputView = view;
	controller->Ds4InputEvent = hEvent;
	return S_OK;
}

static DWORD WINAPI Ds4FfbThreadProc(LPVOID param)
{
	DUO_CONTROLLER* controller = (DUO_CONTROLLER*)param;
	HANDLE waitHandles[2] = { controller->Ds4FfbStopEvent, controller->Ds4OutputEvent };
	while (1)
	{
		DWORD wr = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
		if (wr == WAIT_OBJECT_0)
			break;
		if (wr == WAIT_OBJECT_0 + 1)
		{
			BYTE* outputMem = (BYTE*)controller->Ds4OutputView;
			DUO_CONTROLLER_FORCE_FEEDBACK_REPORT ffReport;
			ZeroMemory(&ffReport, sizeof(ffReport));
			ffReport.Flags = SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_RIGHT_MOTOR_VALID |
				SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_LEFT_MOTOR_VALID;
			ffReport.LeftMotor = outputMem[4];
			ffReport.RightMotor = outputMem[5];
#pragma warning(suppress:4366)
			EnterCriticalSection(&controller->Ds4Cs);
			DuoController_VibrationReportCallback_t callback = controller->VibrationReportCallback;
			void* context = controller->VibrationReportCallbackContext;
#pragma warning(suppress:4366)
			LeaveCriticalSection(&controller->Ds4Cs);
			if (callback != NULL)
				callback(controller, &ffReport, context);
		}
	}
	return 0;
}

static HRESULT Ds4ConnectFfb(DUO_CONTROLLER* controller)
{
	WCHAR sanitized[256];
	SanitizeInstanceIdForPipeName(controller->Ds4InstanceId, sanitized, ARRAYSIZE(sanitized));
	WCHAR mappingName[512];
	swprintf_s(mappingName, ARRAYSIZE(mappingName), L"Global\\Duo_%s_output", sanitized);
	WCHAR eventName[512];
	swprintf_s(eventName, ARRAYSIZE(eventName), L"Global\\Duo_%s_output_event", sanitized);
	ULONGLONG startTime = GetTickCount64();
	HANDLE hMapping = NULL;
	while (GetTickCount64() - startTime < 1000)
	{
		hMapping = OpenFileMappingW(FILE_MAP_READ, FALSE, mappingName);
		if (hMapping != NULL)
			break;
		Sleep(10);
	}
	if (hMapping == NULL)
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	LPVOID view = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
	if (view == NULL)
	{
		CloseHandle(hMapping);
		return DsWin32ErrorToHresult(GetLastError());
	}
	HANDLE hEvent = OpenEventW(SYNCHRONIZE, FALSE, eventName);
	if (hEvent == NULL)
	{
		UnmapViewOfFile(view);
		CloseHandle(hMapping);
		return DsWin32ErrorToHresult(GetLastError());
	}
	controller->Ds4OutputMapping = hMapping;
	controller->Ds4OutputView = view;
	controller->Ds4OutputEvent = hEvent;
	controller->Ds4FfbStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!controller->Ds4FfbStopEvent)
	{
		CloseHandle(hEvent);
		UnmapViewOfFile(view);
		CloseHandle(hMapping);
		controller->Ds4OutputEvent = NULL;
		controller->Ds4OutputView = NULL;
		controller->Ds4OutputMapping = NULL;
		return HRESULT_FROM_WIN32(GetLastError());
	}
	controller->Ds4FfbThread = CreateThread(NULL, 0, Ds4FfbThreadProc, controller, 0, NULL);
	if (!controller->Ds4FfbThread)
	{
		CloseHandle(controller->Ds4FfbStopEvent);
		controller->Ds4FfbStopEvent = NULL;
		CloseHandle(hEvent);
		controller->Ds4OutputEvent = NULL;
		UnmapViewOfFile(view);
		controller->Ds4OutputView = NULL;
		CloseHandle(hMapping);
		controller->Ds4OutputMapping = NULL;
		return HRESULT_FROM_WIN32(GetLastError());
	}
	return S_OK;
}

static void Ds4DisconnectInput(DUO_CONTROLLER* controller)
{
	if (controller->Ds4InputEvent != NULL)
	{
		CloseHandle(controller->Ds4InputEvent);
		controller->Ds4InputEvent = NULL;
	}
	if (controller->Ds4InputView != NULL)
	{
		UnmapViewOfFile(controller->Ds4InputView);
		controller->Ds4InputView = NULL;
	}
	if (controller->Ds4InputMapping != NULL)
	{
		CloseHandle(controller->Ds4InputMapping);
		controller->Ds4InputMapping = NULL;
	}
}

static void Ds4DisconnectFfb(DUO_CONTROLLER* controller)
{
	if (controller->Ds4FfbStopEvent != NULL)
	{
		SetEvent(controller->Ds4FfbStopEvent);
		if (controller->Ds4FfbThread != NULL)
		{
			WaitForSingleObject(controller->Ds4FfbThread, 1000);
			CloseHandle(controller->Ds4FfbThread);
			controller->Ds4FfbThread = NULL;
		}
		CloseHandle(controller->Ds4FfbStopEvent);
		controller->Ds4FfbStopEvent = NULL;
	}
	if (controller->Ds4OutputEvent != NULL)
	{
		CloseHandle(controller->Ds4OutputEvent);
		controller->Ds4OutputEvent = NULL;
	}
	if (controller->Ds4OutputView != NULL)
	{
		UnmapViewOfFile(controller->Ds4OutputView);
		controller->Ds4OutputView = NULL;
	}
	if (controller->Ds4OutputMapping != NULL)
	{
		CloseHandle(controller->Ds4OutputMapping);
		controller->Ds4OutputMapping = NULL;
	}
}

static HRESULT Ds4SendRawInput(DUO_CONTROLLER* controller, const DUO_CONTROLLER_INPUT_REPORT_DS4* state)
{
	if (controller->Ds4InputView == NULL)
		return HRESULT_FROM_WIN32(ERROR_PIPE_NOT_CONNECTED);
	BYTE report[DS4_REPORT_SIZE];
	ZeroMemory(report, DS4_REPORT_SIZE);
	report[0] = DS4_INPUT_REPORT_ID;
	report[1] = state->LeftStickHorizontal;
	report[2] = state->LeftStickVertical;
	report[3] = state->RightStickHorizontal;
	report[4] = state->RightStickVertical;
	report[5] = (state->DPad & 0x0F);
	if (state->Square)   report[5] |= 0x10;
	if (state->Cross)    report[5] |= 0x20;
	if (state->Circle)   report[5] |= 0x40;
	if (state->Triangle) report[5] |= 0x80;
	if (state->L1)        report[6] |= 0x01;
	if (state->R1)        report[6] |= 0x02;
	if (state->L2)        report[6] |= 0x04;
	if (state->R2)        report[6] |= 0x08;
	if (state->Share)     report[6] |= 0x10;
	if (state->Options)   report[6] |= 0x20;
	if (state->L3)        report[6] |= 0x40;
	if (state->R3)        report[6] |= 0x80;
	if (state->PS)        report[7] |= 0x01;
	if (state->Touchpad)  report[7] |= 0x02;
	report[8] = state->LeftTrigger;
	report[9] = state->RightTrigger;
	report[12] = 0xFF;
	memcpy(&report[13], &state->AngularVelocityX, sizeof(INT16));
	memcpy(&report[15], &state->AngularVelocityY, sizeof(INT16));
	memcpy(&report[17], &state->AngularVelocityZ, sizeof(INT16));
	memcpy(&report[19], &state->AccelerometerX, sizeof(INT16));
	memcpy(&report[21], &state->AccelerometerY, sizeof(INT16));
	memcpy(&report[23], &state->AccelerometerZ, sizeof(INT16));
	report[30] = 0x1A;
	report[35] = state->TouchData.Finger[0].NotTouching ? 0xFF : (BYTE)state->TouchData.Finger[0].Index;
	USHORT t1x = min(state->TouchData.Finger[0].FingerX, DS4_TOUCHPAD_MAX_X) & 0xFFF;
	USHORT t1y = min(state->TouchData.Finger[0].FingerY, DS4_TOUCHPAD_MAX_Y) & 0xFFF;
	UINT t1 = ((UINT)t1y << 12) | t1x;
	report[36] = (BYTE)(t1 & 0xFF);
	report[37] = (BYTE)((t1 >> 8) & 0xFF);
	report[38] = (BYTE)((t1 >> 16) & 0xFF);
	report[39] = state->TouchData.Finger[1].NotTouching ? 0xFF : (BYTE)state->TouchData.Finger[1].Index;
	USHORT t2x = min(state->TouchData.Finger[1].FingerX, DS4_TOUCHPAD_MAX_X) & 0xFFF;
	USHORT t2y = min(state->TouchData.Finger[1].FingerY, DS4_TOUCHPAD_MAX_Y) & 0xFFF;
	UINT t2 = ((UINT)t2y << 12) | t2x;
	report[40] = (BYTE)(t2 & 0xFF);
	report[41] = (BYTE)((t2 >> 8) & 0xFF);
	report[42] = (BYTE)((t2 >> 16) & 0xFF);
	BYTE* inputMem = (BYTE*)controller->Ds4InputView;
	inputMem[0] = INPUT_REPORT_FULL;
	inputMem[1] = DS4_REPORT_SIZE;
	memcpy(&inputMem[MESSAGE_HEADER_LEN], report, DS4_REPORT_SIZE);
	if (!SetEvent(controller->Ds4InputEvent))
		return DsWin32ErrorToHresult(GetLastError());
	return S_OK;
}

static HRESULT CreateDualShock4Controller(DUO_CONTROLLER* controller)
{
	WCHAR instanceId[256];
	WCHAR hidInstanceId[256];
	WCHAR token[40];
	GenerateUniqueDeviceToken(token, ARRAYSIZE(token));
	WCHAR seed[96];
	swprintf_s(seed, ARRAYSIZE(seed), L"VID_054C&PID_05C4&DUOCONTROLLER&%s", token);
	HRESULT result = InstallDuoControllerDevice(L"Root\\VID_054C&PID_05C4", seed, instanceId, ARRAYSIZE(instanceId), hidInstanceId, ARRAYSIZE(hidInstanceId));
	if (FAILED(result))
		return result;
	wcscpy_s(controller->Ds4InstanceId, ARRAYSIZE(controller->Ds4InstanceId), instanceId);
	wcscpy_s(controller->Ds4HidInstanceId, ARRAYSIZE(controller->Ds4HidInstanceId), hidInstanceId);
	controller->Ds4InputMapping = NULL;
	controller->Ds4OutputMapping = NULL;
	controller->Ds4InputView = NULL;
	controller->Ds4OutputView = NULL;
	controller->Ds4InputEvent = NULL;
	controller->Ds4OutputEvent = NULL;
	controller->Ds4FfbThread = NULL;
	controller->Ds4FfbStopEvent = NULL;
#pragma warning(suppress:4366)
	InitializeCriticalSection(&controller->Ds4Cs);
	result = Ds4ConnectInput(controller);
	if (FAILED(result))
	{
		RemoveDualShock4Controller(controller);
		return result;
	}
	result = Ds4ConnectFfb(controller);
	if (FAILED(result))
	{
		RemoveDualShock4Controller(controller);
		return result;
	}
	return S_OK;
}

static HRESULT RemoveDualShock4Controller(DUO_CONTROLLER* controller)
{
	Ds4DisconnectFfb(controller);
	Ds4DisconnectInput(controller);
#pragma warning(suppress:4366)
	DeleteCriticalSection(&controller->Ds4Cs);
	return RemoveDuoControllerDevice(controller->Ds4InstanceId);
}

static HRESULT SendDs4Report(DUO_CONTROLLER* controller, DUO_CONTROLLER_INPUT_REPORT_DS4* inputReport)
{
#pragma warning(suppress:4366)
	EnterCriticalSection(&controller->Ds4Cs);
	HRESULT result = Ds4SendRawInput(controller, inputReport);
	if (SUCCEEDED(result))
		controller->LastDs4InputReport = *inputReport;
#pragma warning(suppress:4366)
	LeaveCriticalSection(&controller->Ds4Cs);
	return result;
}

// ==================== Xbox shared memory helpers ====================

static HRESULT XboxConnectInput(DUO_CONTROLLER* controller)
{
	WCHAR sanitized[256];
	SanitizeInstanceIdForPipeName(controller->XboxInstanceId, sanitized, ARRAYSIZE(sanitized));
	WCHAR mappingName[512];
	swprintf_s(mappingName, ARRAYSIZE(mappingName), L"Global\\Duo_%s_input", sanitized);
	WCHAR eventName[512];
	swprintf_s(eventName, ARRAYSIZE(eventName), L"Global\\Duo_%s_input_event", sanitized);
#ifdef _DEBUG
	wprintf(L"[XboxInput] Connecting input: mapping=%s event=%s\n", mappingName, eventName);
#endif
	ULONGLONG startTime = GetTickCount64();
	HANDLE hMapping = NULL;
	while (GetTickCount64() - startTime < 1000)
	{
		hMapping = OpenFileMappingW(FILE_MAP_WRITE, FALSE, mappingName);
		if (hMapping != NULL)
			break;
		Sleep(10);
	}
	if (hMapping == NULL)
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	LPVOID view = MapViewOfFile(hMapping, FILE_MAP_WRITE, 0, 0, 0);
	if (view == NULL)
	{
		CloseHandle(hMapping);
		return DsWin32ErrorToHresult(GetLastError());
	}
	HANDLE hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, eventName);
	if (hEvent == NULL)
	{
		UnmapViewOfFile(view);
		CloseHandle(hMapping);
		return DsWin32ErrorToHresult(GetLastError());
	}
	controller->XboxInputMapping = hMapping;
	controller->XboxInputView = view;
	controller->XboxInputEvent = hEvent;
	return S_OK;
}

static DWORD WINAPI XboxFfbThreadProc(LPVOID param)
{
	DUO_CONTROLLER* controller = (DUO_CONTROLLER*)param;
	HANDLE waitHandles[2] = { controller->XboxFfbStopEvent, controller->XboxOutputEvent };
#ifdef _DEBUG
	wprintf(L"[XboxFfb] FFB thread started, waiting for output events\n");
#endif
	while (1)
	{
		DWORD wr = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
		if (wr == WAIT_OBJECT_0)
		{
#ifdef _DEBUG
			wprintf(L"[XboxFfb] FFB thread stopping (stop event signaled)\n");
#endif
			break;
		}
		if (wr == WAIT_OBJECT_0 + 1)
		{
			BYTE* outputMem = (BYTE*)controller->XboxOutputView;
#ifdef _DEBUG
			wprintf(L"[XboxFfb] Output event signaled! Raw bytes: [%02X %02X %02X %02X %02X %02X %02X %02X %02X]\n",
				outputMem[0], outputMem[1], outputMem[2], outputMem[3],
				outputMem[4], outputMem[5], outputMem[6], outputMem[7],
				outputMem[8]);
#endif
			DUO_CONTROLLER_FORCE_FEEDBACK_REPORT ffReport;
			ZeroMemory(&ffReport, sizeof(ffReport));
			if (outputMem[0] == XB1_OUTPUT_REPORT_ID)
			{
				ffReport.Flags = outputMem[1];
				ffReport.LeftTrigger = outputMem[2];
				ffReport.RightTrigger = outputMem[3];
				ffReport.LeftMotor = outputMem[4];
				ffReport.RightMotor = outputMem[5];
				ffReport.Duration = outputMem[6];
				ffReport.StartDelay = outputMem[7];
				ffReport.Loop = outputMem[8];
#ifdef _DEBUG
				wprintf(L"[XboxFfb] PID report! enable=0x%02X LeftMotor=%d RightMotor=%d LTrig=%d RTrig=%d Dur=%d StartDelay=%d Loop=%d\n",
					ffReport.Flags, ffReport.LeftMotor, ffReport.RightMotor, ffReport.LeftTrigger, ffReport.RightTrigger,
					ffReport.Duration, ffReport.StartDelay, ffReport.Loop);
#endif
			}
#ifdef _DEBUG
			else
			{
				wprintf(L"[XboxFfb] Report ID MISMATCH: expected 0x%02X, got 0x%02X\n", XB1_OUTPUT_REPORT_ID, outputMem[0]);
			}
#endif
#pragma warning(suppress:4366)
			EnterCriticalSection(&controller->XboxCs);
			DuoController_VibrationReportCallback_t callback = controller->VibrationReportCallback;
			void* context = controller->VibrationReportCallbackContext;
#pragma warning(suppress:4366)
			LeaveCriticalSection(&controller->XboxCs);
			if (callback != NULL)
			{
#ifdef _DEBUG
				wprintf(L"[XboxFfb] Invoking callback: LeftMotor=%d RightMotor=%d Flags=0x%02X\n",
					ffReport.LeftMotor, ffReport.RightMotor, ffReport.Flags);
#endif
				callback(controller, &ffReport, context);
			}
#ifdef _DEBUG
			else
			{
				wprintf(L"[XboxFfb] No callback registered!\n");
			}
#endif
		}
		else
		{
#ifdef _DEBUG
			wprintf(L"[XboxFfb] WaitForMultipleObjects unexpected result: %d (GLE=%d)\n", wr, GetLastError());
#endif
		}
	}
	return 0;
}

static HRESULT XboxConnectFfb(DUO_CONTROLLER* controller)
{
	WCHAR sanitized[256];
	SanitizeInstanceIdForPipeName(controller->XboxInstanceId, sanitized, ARRAYSIZE(sanitized));
	WCHAR mappingName[512];
	swprintf_s(mappingName, ARRAYSIZE(mappingName), L"Global\\Duo_%s_output", sanitized);
	WCHAR eventName[512];
	swprintf_s(eventName, ARRAYSIZE(eventName), L"Global\\Duo_%s_output_event", sanitized);
#ifdef _DEBUG
	wprintf(L"[XboxFfb] Connecting FFB: mapping=%s event=%s\n", mappingName, eventName);
#endif
	ULONGLONG startTime = GetTickCount64();
	HANDLE hMapping = NULL;
	while (GetTickCount64() - startTime < 1000)
	{
		hMapping = OpenFileMappingW(FILE_MAP_READ, FALSE, mappingName);
		if (hMapping != NULL)
			break;
		Sleep(10);
	}
	if (hMapping == NULL)
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	LPVOID view = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
	if (view == NULL)
	{
		CloseHandle(hMapping);
		return DsWin32ErrorToHresult(GetLastError());
	}
	HANDLE hEvent = OpenEventW(SYNCHRONIZE, FALSE, eventName);
	if (hEvent == NULL)
	{
		UnmapViewOfFile(view);
		CloseHandle(hMapping);
		return DsWin32ErrorToHresult(GetLastError());
	}
	controller->XboxOutputMapping = hMapping;
	controller->XboxOutputView = view;
	controller->XboxOutputEvent = hEvent;
	controller->XboxFfbStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!controller->XboxFfbStopEvent)
	{
		CloseHandle(hEvent);
		UnmapViewOfFile(view);
		CloseHandle(hMapping);
		controller->XboxOutputEvent = NULL;
		controller->XboxOutputView = NULL;
		controller->XboxOutputMapping = NULL;
		return HRESULT_FROM_WIN32(GetLastError());
	}
	controller->XboxFfbThread = CreateThread(NULL, 0, XboxFfbThreadProc, controller, 0, NULL);
	if (!controller->XboxFfbThread)
	{
		CloseHandle(controller->XboxFfbStopEvent);
		controller->XboxFfbStopEvent = NULL;
		CloseHandle(hEvent);
		controller->XboxOutputEvent = NULL;
		UnmapViewOfFile(view);
		controller->XboxOutputView = NULL;
		CloseHandle(hMapping);
		controller->XboxOutputMapping = NULL;
		return HRESULT_FROM_WIN32(GetLastError());
	}
	return S_OK;
}

// ==================== Xbox debug shared memory ====================

#ifdef _DEBUG
static DWORD WINAPI XboxDebugThreadProc(LPVOID param)
{
	DUO_CONTROLLER* controller = (DUO_CONTROLLER*)param;
	PDEBUG_RING_BUFFER ring = (PDEBUG_RING_BUFFER)controller->XboxDebugView;
	if (ring == NULL)
		return 1;

	wprintf(L"[XboxDebug] Debug reader thread started\n");

	LONG lastRead = 0;
	while (WaitForSingleObject(controller->XboxDebugStopEvent, 100) == WAIT_TIMEOUT)
	{
		LONG writeIdx = ring->WriteIndex;
		while (lastRead < writeIdx)
		{
			LONG slot = lastRead % DEBUG_MSG_SLOT_COUNT;
			wprintf(L"  %hs\n", ring->Messages[slot]);
			lastRead++;
		}
	}
	return 0;
}

static HRESULT XboxConnectDebug(DUO_CONTROLLER* controller)
{
	WCHAR sanitized[256];
	SanitizeInstanceIdForPipeName(controller->XboxInstanceId, sanitized, ARRAYSIZE(sanitized));
	WCHAR mappingName[512];
	swprintf_s(mappingName, ARRAYSIZE(mappingName), L"Global\\Duo_%s_debug", sanitized);

	controller->XboxDebugMapping = NULL;
	controller->XboxDebugView = NULL;
	controller->XboxDebugThread = NULL;
	controller->XboxDebugStopEvent = NULL;

	ULONGLONG startTime = GetTickCount64();
	HANDLE hMapping = NULL;
	while (GetTickCount64() - startTime < 1000)
	{
		hMapping = OpenFileMappingW(FILE_MAP_READ, FALSE, mappingName);
		if (hMapping != NULL)
			break;
		Sleep(10);
	}
	if (hMapping == NULL)
	{
		wprintf(L"[XboxDebug] Could not open debug shared memory: %s (error %d)\n", mappingName, GetLastError());
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	}
	LPVOID view = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, DEBUG_SHM_SIZE);
	if (view == NULL)
	{
		wprintf(L"[XboxDebug] MapViewOfFile failed: %d\n", GetLastError());
		CloseHandle(hMapping);
		return DsWin32ErrorToHresult(GetLastError());
	}

	controller->XboxDebugMapping = hMapping;
	controller->XboxDebugView = view;

	controller->XboxDebugStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (controller->XboxDebugStopEvent == NULL)
	{
		UnmapViewOfFile(view);
		controller->XboxDebugView = NULL;
		CloseHandle(hMapping);
		controller->XboxDebugMapping = NULL;
		return DsWin32ErrorToHresult(GetLastError());
	}

	controller->XboxDebugThread = CreateThread(NULL, 0, XboxDebugThreadProc, controller, 0, NULL);
	wprintf(L"[XboxDebug] Connected to debug shared memory: %s\n", mappingName);
	return S_OK;
}

static void XboxDisconnectDebug(DUO_CONTROLLER* controller)
{
	if (controller->XboxDebugStopEvent != NULL)
	{
		SetEvent(controller->XboxDebugStopEvent);
		if (controller->XboxDebugThread != NULL)
		{
			WaitForSingleObject(controller->XboxDebugThread, 2000);
			CloseHandle(controller->XboxDebugThread);
			controller->XboxDebugThread = NULL;
		}
		CloseHandle(controller->XboxDebugStopEvent);
		controller->XboxDebugStopEvent = NULL;
	}
	if (controller->XboxDebugView != NULL)
	{
		UnmapViewOfFile(controller->XboxDebugView);
		controller->XboxDebugView = NULL;
	}
	if (controller->XboxDebugMapping != NULL)
	{
		CloseHandle(controller->XboxDebugMapping);
		controller->XboxDebugMapping = NULL;
	}
}
#endif

static void XboxDisconnectInput(DUO_CONTROLLER* controller)
{
	if (controller->XboxInputEvent != NULL)
	{
		CloseHandle(controller->XboxInputEvent);
		controller->XboxInputEvent = NULL;
	}
	if (controller->XboxInputView != NULL)
	{
		UnmapViewOfFile(controller->XboxInputView);
		controller->XboxInputView = NULL;
	}
	if (controller->XboxInputMapping != NULL)
	{
		CloseHandle(controller->XboxInputMapping);
		controller->XboxInputMapping = NULL;
	}
}

static void XboxDisconnectFfb(DUO_CONTROLLER* controller)
{
	if (controller->XboxFfbStopEvent != NULL)
	{
		SetEvent(controller->XboxFfbStopEvent);
		if (controller->XboxFfbThread != NULL)
		{
			WaitForSingleObject(controller->XboxFfbThread, 1000);
			CloseHandle(controller->XboxFfbThread);
			controller->XboxFfbThread = NULL;
		}
		CloseHandle(controller->XboxFfbStopEvent);
		controller->XboxFfbStopEvent = NULL;
	}
	if (controller->XboxOutputEvent != NULL)
	{
		CloseHandle(controller->XboxOutputEvent);
		controller->XboxOutputEvent = NULL;
	}
	if (controller->XboxOutputView != NULL)
	{
		UnmapViewOfFile(controller->XboxOutputView);
		controller->XboxOutputView = NULL;
	}
	if (controller->XboxOutputMapping != NULL)
	{
		CloseHandle(controller->XboxOutputMapping);
		controller->XboxOutputMapping = NULL;
	}
}

static HRESULT XboxSendRawInput(DUO_CONTROLLER* controller, const DUO_CONTROLLER_INPUT_REPORT_XBOX* state)
{
	if (controller->XboxInputView == NULL)
		return HRESULT_FROM_WIN32(ERROR_PIPE_NOT_CONNECTED);
	BYTE report[XB1_REPORT_SIZE];
	ZeroMemory(report, XB1_REPORT_SIZE);
	report[0] = XB1_INPUT_REPORT_ID;
	memcpy(&report[1], &state->LeftStickHorizontal, sizeof(UINT16));
	memcpy(&report[3], &state->LeftStickVertical, sizeof(UINT16));
	memcpy(&report[5], &state->RightStickHorizontal, sizeof(UINT16));
	memcpy(&report[7], &state->RightStickVertical, sizeof(UINT16));

	// Left trigger Z (10-bit at bits 72-81 = bytes 9-10:0-1)
	{
		UINT16 zVal = (UINT16)state->LeftTrigger << 2;
		report[9] = (BYTE)(zVal & 0xFF);
		report[10] = (BYTE)((zVal >> 8) & 0x03);
	}

	// Right trigger Rz (10-bit at bits 88-97 = bytes 11-12:0-1)
	{
		UINT16 rzVal = (UINT16)state->RightTrigger << 2;
		report[11] = (BYTE)(rzVal & 0xFF);
		report[12] = (BYTE)((rzVal >> 8) & 0x03);
	}

	// DPad (0-8 in 45° increments clockwise)
	report[13] = state->DPad;

	// These work already
	if (state->A)      report[14] |= (1 << XB1_BUTTON_A);
	if (state->B)      report[14] |= (1 << XB1_BUTTON_B);
	if (state->X)      report[14] |= (1 << XB1_BUTTON_X);
	if (state->Y)      report[14] |= (1 << XB1_BUTTON_Y);
	if (state->LeftBumper)  report[14] |= (1 << XB1_BUTTON_LB);
	if (state->RightBumper) report[14] |= (1 << XB1_BUTTON_RB);
	if (state->Back)   report[14] |= (1 << XB1_BUTTON_BACK);
	if (state->Start)  report[14] |= (1 << XB1_BUTTON_START);
	if (state->LeftStick)   report[15] |= (1 << (XB1_BUTTON_LSB - 8));
	if (state->RightStick)  report[15] |= (1 << (XB1_BUTTON_RSB - 8));
	if (state->Guide)  report[15] |= (1 << (XB1_BUTTON_GUIDE - 8));
	if (state->Paddle1) report[15] |= (1 << (XB1_BUTTON_PADDLE1 - 8));
	if (state->Paddle2) report[15] |= (1 << (XB1_BUTTON_PADDLE2 - 8));
	if (state->Paddle3) report[15] |= (1 << (XB1_BUTTON_PADDLE3 - 8));
	if (state->Paddle4) report[15] |= (1 << (XB1_BUTTON_PADDLE4 - 8));

	BYTE* inputMem = (BYTE*)controller->XboxInputView;
	inputMem[0] = INPUT_REPORT_FULL;
	inputMem[1] = XB1_REPORT_SIZE;
	memcpy(&inputMem[MESSAGE_HEADER_LEN], report, XB1_REPORT_SIZE);
	if (!SetEvent(controller->XboxInputEvent))
		return DsWin32ErrorToHresult(GetLastError());
	return S_OK;
}

static HRESULT CreateXboxController(DUO_CONTROLLER* controller)
{
	WCHAR instanceId[256];
	WCHAR hidInstanceId[256];
	WCHAR token[40];
	GenerateUniqueDeviceToken(token, ARRAYSIZE(token));
	WCHAR seed[96];
	swprintf_s(seed, ARRAYSIZE(seed), L"VID_045E&PID_02FF&DUOCONTROLLER&%s", token);
	HRESULT result = InstallDuoControllerDevice(L"Root\\VID_045E&PID_02FF&IG_00", seed, instanceId, ARRAYSIZE(instanceId), hidInstanceId, ARRAYSIZE(hidInstanceId));
	if (FAILED(result))
		return result;
	wcscpy_s(controller->XboxInstanceId, ARRAYSIZE(controller->XboxInstanceId), instanceId);
	wcscpy_s(controller->XboxHidInstanceId, ARRAYSIZE(controller->XboxHidInstanceId), hidInstanceId);
	controller->XboxInputMapping = NULL;
	controller->XboxOutputMapping = NULL;
	controller->XboxInputView = NULL;
	controller->XboxOutputView = NULL;
	controller->XboxInputEvent = NULL;
	controller->XboxOutputEvent = NULL;
	controller->XboxFfbThread = NULL;
	controller->XboxFfbStopEvent = NULL;
#ifdef _DEBUG
	controller->XboxDebugMapping = NULL;
	controller->XboxDebugView = NULL;
	controller->XboxDebugThread = NULL;
	controller->XboxDebugStopEvent = NULL;
#endif
#pragma warning(suppress:4366)
	InitializeCriticalSection(&controller->XboxCs);
	result = XboxConnectInput(controller);
	if (FAILED(result))
	{
		RemoveXboxController(controller);
		return result;
	}
	result = XboxConnectFfb(controller);
	if (FAILED(result))
	{
		RemoveXboxController(controller);
		return result;
	}
#ifdef _DEBUG
	XboxConnectDebug(controller); // best-effort, non-fatal
#endif
	return S_OK;
}

static HRESULT RemoveXboxController(DUO_CONTROLLER* controller)
{
#ifdef _DEBUG
	XboxDisconnectDebug(controller);
#endif
	XboxDisconnectFfb(controller);
	XboxDisconnectInput(controller);
#pragma warning(suppress:4366)
	DeleteCriticalSection(&controller->XboxCs);
	return RemoveDuoControllerDevice(controller->XboxInstanceId);
}

static HRESULT SendXboxReport(DUO_CONTROLLER* controller, DUO_CONTROLLER_INPUT_REPORT_XBOX* inputReport)
{
#pragma warning(suppress:4366)
	EnterCriticalSection(&controller->XboxCs);
	HRESULT result = XboxSendRawInput(controller, inputReport);
	if (SUCCEEDED(result))
		controller->LastXboxInputReport = *inputReport;
#pragma warning(suppress:4366)
	LeaveCriticalSection(&controller->XboxCs);
	return result;
}

// ==================== Exported API ====================

HRESULT WINAPI DuoController_Initialize()
{
	HRESULT result = S_OK;
	InitializeWindowsRuntimeForCurrentThread();
	if (!Initialized)
	{
		Initialized = TRUE;
	}
	return result;
}

HRESULT WINAPI DuoController_Uninitialize()
{
	HRESULT result = S_OK;
	InitializeWindowsRuntimeForCurrentThread();
	if (Initialized)
	{
		while (Controllers != NULL)
		{
			DuoController_RemoveController(Controllers[0]);
		}
		Initialized = FALSE;
	}
	else
	{
		result = E_UNEXPECTED;
	}
	return result;
}

HRESULT WINAPI DuoController_CreateController(DUO_CONTROLLER_TYPE controllerType, DuoController_VibrationReportCallback_t vibrationCallback, void* vibrationCallbackContext, void** controller)
{
	HRESULT result = S_OK;
	InitializeWindowsRuntimeForCurrentThread();
	if (Initialized)
	{
		if (controller != NULL)
		{
			DUO_CONTROLLER* newController = (DUO_CONTROLLER*)malloc(sizeof(DUO_CONTROLLER));
			if (newController != NULL)
			{
				memset(newController, 0, sizeof(DUO_CONTROLLER));
				newController->Type = controllerType;
				newController->VibrationReportCallback = vibrationCallback;
				newController->VibrationReportCallbackContext = vibrationCallbackContext;
				if (controllerType == DuoControllerTypeXbox)
				{
					ZeroMemory(&newController->LastXboxInputReport, sizeof(newController->LastXboxInputReport));
					result = CreateXboxController(newController);
					if (SUCCEEDED(result))
					{
						DUO_CONTROLLER** newControllers = (DUO_CONTROLLER**)realloc(Controllers, sizeof(DUO_CONTROLLER*) * (ControllerCount + 1));
						if (newControllers)
						{
							Controllers = newControllers;
							Controllers[ControllerCount++] = newController;
							*controller = newController;
							Sleep(1000);
							DuoController_SendReport(newController, &newController->LastXboxInputReport);
						}
						else
						{
							RemoveXboxController(newController);
							result = E_OUTOFMEMORY;
						}
					}
				}
				else if (controllerType == DuoControllerTypeDualShock4)
				{
					result = CreateDualShock4Controller(newController);
					if (SUCCEEDED(result))
					{
						DUO_CONTROLLER** newControllers = (DUO_CONTROLLER**)realloc(Controllers, sizeof(DUO_CONTROLLER*) * (ControllerCount + 1));
						if (newControllers)
						{
							Controllers = newControllers;
							Controllers[ControllerCount++] = newController;
							*controller = newController;
						}
						else
						{
							RemoveDualShock4Controller(newController);
							result = E_OUTOFMEMORY;
						}
					}
				}
				else if (controllerType == DuoControllerTypeDualSense || controllerType == DuoControllerTypeDualSenseEdge)
				{
					USHORT pid = (controllerType == DuoControllerTypeDualSense) ? 0x0CE6 : 0x0DF2;
					result = CreateDualSenseController(newController, pid);
					if (SUCCEEDED(result))
					{
						DUO_CONTROLLER** newControllers = (DUO_CONTROLLER**)realloc(Controllers, sizeof(DUO_CONTROLLER*) * (ControllerCount + 1));
						if (newControllers)
						{
							Controllers = newControllers;
							Controllers[ControllerCount++] = newController;
							*controller = newController;
						}
						else
						{
							RemoveDualSenseController(newController);
							result = E_OUTOFMEMORY;
						}
					}
				}
				else
				{
					result = E_INVALIDARG;
				}
				if (result != S_OK)
				{
					free(newController);
				}
			}
			else
			{
				return E_OUTOFMEMORY;
			}
		}
		else
		{
			result = E_INVALIDARG;
		}
	}
	else
	{
		result = E_UNEXPECTED;
	}
	return result;
}

HRESULT WINAPI DuoController_RemoveController(void* controller)
{
	HRESULT result = S_OK;
	InitializeWindowsRuntimeForCurrentThread();
	if (Initialized)
	{
		DUO_CONTROLLER* duoController = (DUO_CONTROLLER*)controller;
		result = E_INVALIDARG;
		for (DWORD i = 0; i < ControllerCount; i++)
		{
			if (Controllers[i] == duoController)
			{
				for (DWORD j = i; j < ControllerCount - 1; j++)
					Controllers[j] = Controllers[j + 1];
				ControllerCount--;
				if (ControllerCount > 0)
				{
					DUO_CONTROLLER** newControllers = (DUO_CONTROLLER**)realloc(Controllers, sizeof(DUO_CONTROLLER*) * ControllerCount);
					if (newControllers != NULL)
						Controllers = newControllers;
				}
				else
				{
					free(Controllers);
					Controllers = NULL;
				}
				if (duoController->Type == DuoControllerTypeXbox)
				{
					RemoveXboxController(duoController);
				}
				else if (duoController->Type == DuoControllerTypeDualSenseEdge || duoController->Type == DuoControllerTypeDualSense)
				{
					RemoveDualSenseController(duoController);
				}
				else if (duoController->Type == DuoControllerTypeDualShock4)
				{
					RemoveDualShock4Controller(duoController);
				}
				free(duoController);
				result = S_OK;
				break;
			}
		}
	}
	else
	{
		result = E_UNEXPECTED;
	}
	return result;
}

HRESULT WINAPI DuoController_SendReport(void* controller, void* inputReport)
{
	HRESULT result = S_OK;
	InitializeWindowsRuntimeForCurrentThread();
	if (Initialized)
	{
		if (controller != NULL && inputReport != NULL)
		{
			DUO_CONTROLLER* duoController = (DUO_CONTROLLER*)controller;
			if (duoController->Type == DuoControllerTypeXbox)
			{
				result = SendXboxReport(duoController, (DUO_CONTROLLER_INPUT_REPORT_XBOX*)inputReport);
			}
			else if (duoController->Type == DuoControllerTypeDualSenseEdge || duoController->Type == DuoControllerTypeDualSense)
			{
				result = SendDsReport(duoController, (DUO_CONTROLLER_INPUT_REPORT_DUALSENSE*)inputReport);
			}
			else if (duoController->Type == DuoControllerTypeDualShock4)
			{
				result = SendDs4Report(duoController, (DUO_CONTROLLER_INPUT_REPORT_DS4*)inputReport);
			}
			else
			{
				result = E_INVALIDARG;
			}
		}
		else
		{
			result = E_INVALIDARG;
		}
	}
	else
	{
		result = E_UNEXPECTED;
	}
	return result;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	(void)hModule;
	(void)lpReserved;
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		ProcessIdToSessionId(GetCurrentProcessId(), &SessionId);
	}
	return TRUE;
}
