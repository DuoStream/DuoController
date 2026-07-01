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
#include <winternl.h>
#include <winstring.h>
#include <initguid.h>
#include <devpkey.h>
#include <devquery.h>
#include <devquerydef.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <newdev.h>
#include <stdio.h>
#include "Public.h"

#pragma comment(lib, "mincore.lib")
#pragma comment(lib, "onecore.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

#ifndef STATUS_WAIT_0
#define STATUS_WAIT_0 ((DWORD)0x00000000L)
#endif

/// <summary>
/// A dummy value used to target no process, essentially acting as a input silencer.
/// </summary>
#define NO_PROCESS 0x100000000000000

/// <summary>
/// Controller types supported by xboxgipsynthetic.dll.
/// </summary>
typedef enum _SYNTHETIC_CONTROLLER_TYPE
{
	SyntheticControllerTypeGamepad = 0,
	SyntheticControllerTypeGipStreamGamepad = 1,
	SyntheticControllerTypeExtendedGamepad = 2
} SYNTHETIC_CONTROLLER_TYPE;

/// <summary>
/// The supported synthetic controller input report types.
/// </summary>
typedef enum _SYNTHETIC_CONTROLLER_INPUT_REPORT_TYPE
{
	SYNTHETIC_CONTROLLER_INPUT_REPORT_TYPE_CONTROLLER = 0,
	SYNTHETIC_CONTROLLER_INPUT_REPORT_TYPE_VKEY = 1,
	SYNTHETIC_CONTROLLER_INPUT_REPORT_TYPE_GIP_STREAM = 2,
	SYNTHETIC_CONTROLLER_INPUT_REPORT_TYPE_EXTENDED_CONTROLLER = 3
} SYNTHETIC_CONTROLLER_INPUT_REPORT_TYPE;

/// <summary>
/// The supported synthetic controller output report types.
/// </summary>
typedef enum _SYNTHETIC_CONTROLLER_OUTPUT_REPORT_TYPE
{
	SYNTHETIC_CONTROLLER_OUTPUT_REPORT_TYPE_CONTROLLER = 0
} SYNTHETIC_CONTROLLER_OUTPUT_REPORT_TYPE;

#pragma pack(push, 1)

/// <summary>
/// The virtual key input report structure used to report the Guide button state.
/// </summary>
typedef struct _SYNTHETIC_CONTROLLER_VKEY_INPUT_REPORT
{
	INT16 VirtualKey; // Must be set to 0, internally hardcoded to VK_LWIN
	UINT8 State; // 0 = Released, 1 = Pressed
} SYNTHETIC_CONTROLLER_VKEY_INPUT_REPORT;

/// <summary>
/// The DuoController structure.
/// </summary>
typedef struct _DUO_CONTROLLER
{
	DUO_CONTROLLER_TYPE Type;

	DuoController_VibrationReportCallback_t VibrationReportCallback;
	void* VibrationReportCallbackContext;

	// Xbox-specific fields
	void* SyntheticHandle;
	HDEVQUERY DeviceQuery;
	DUO_CONTROLLER_INPUT_REPORT_XBOX LastXboxInputReport;

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

/// <summary>
/// Defines a WNF type ID.
/// </summary>
typedef struct _WNF_TYPE_ID {
	GUID TypeId;
} WNF_TYPE_ID, * PWNF_TYPE_ID;

/// <summary>
/// Defines a WNF state name.
/// </summary>
typedef struct _WNF_STATE_NAME {
	ULONG Data[2];
} WNF_STATE_NAME, * PWNF_STATE_NAME;

/// <summary>
/// The WNF state name used by ISM.dll (dwm.exe) to report focus changes.
/// </summary>
static WNF_STATE_NAME WNF_SHEL_FOCUS_CHANGE = { 0xA3BC7875, 0xD83063E };

typedef void(WINAPI *SyntheticController_ReportCallback_t)(void* controller, unsigned int outputReportType, void* outputReportBuffer, unsigned int outputReportBufferSize, void* context);
typedef HRESULT(WINAPI *SyntheticController_CreateController_t)(SYNTHETIC_CONTROLLER_TYPE controllerType, void** controller);
typedef HRESULT(WINAPI *SyntheticController_SetTargetProcess_t)(void* controller, unsigned long long processId /* or NO_PROCESS */, SYNTHETIC_CONTROLLER_INPUT_REPORT_TYPE inputReportType, void* inputReportBuffer, unsigned int inputReportBufferSize);
typedef HRESULT(WINAPI *SyntheticController_RegisterReportCallback_t)(void* controller, SYNTHETIC_CONTROLLER_OUTPUT_REPORT_TYPE outputReportType, SyntheticController_ReportCallback_t callback, void* context);
typedef HRESULT(WINAPI *SyntheticController_Connect_t)(void* controller);
typedef HRESULT(WINAPI *SyntheticController_SendReport_t)(void* controller, SYNTHETIC_CONTROLLER_INPUT_REPORT_TYPE inputReportType, void* inputReportBuffer, unsigned int inputReportBufferSize);
typedef HRESULT(WINAPI *SyntheticController_Disconnect_t)(void* controller);
typedef HRESULT(WINAPI *SyntheticController_UnregisterReportCallback_t)(void* controller, SYNTHETIC_CONTROLLER_OUTPUT_REPORT_TYPE outputReportType);
typedef HRESULT(WINAPI *SyntheticController_RemoveController_t)(void* controller);
typedef HRESULT(WINAPI *SyntheticController_GetDeviceId_t)(void* controller, unsigned long long* deviceId);
typedef HRESULT(WINAPI* DevSetObjectProperties_t)(DEV_OBJECT_TYPE ObjectType, PCWSTR pszObjectId, ULONG pcPropertyCount, const DEVPROPERTY* ppProperties);
typedef NTSTATUS(WINAPI* RtlPublishWnfStateData_t)(WNF_STATE_NAME StateName, const PWNF_TYPE_ID TypeId, const VOID* Buffer, ULONG Length, const VOID* ExplicitScope);

SyntheticController_CreateController_t SyntheticController_CreateController;
SyntheticController_SetTargetProcess_t SyntheticController_SetTargetProcess;
SyntheticController_RegisterReportCallback_t SyntheticController_RegisterReportCallback;
SyntheticController_Connect_t SyntheticController_Connect;
SyntheticController_SendReport_t SyntheticController_SendReport;
SyntheticController_Disconnect_t SyntheticController_Disconnect;
SyntheticController_UnregisterReportCallback_t SyntheticController_UnregisterReportCallback;
SyntheticController_RemoveController_t SyntheticController_RemoveController;
SyntheticController_GetDeviceId_t SyntheticController_GetDeviceId;
DevSetObjectProperties_t DevSetObjectProperties;
RtlPublishWnfStateData_t RtlPublishWnfStateData;

static DWORD SessionId;
static BOOL Initialized;
static DWORD ForegroundWindowProcessId;
static DUO_CONTROLLER** Controllers = NULL;
static DWORD ControllerCount = 0;

static void InitializeWindowsRuntimeForCurrentThread()
{
	const wchar_t* type_name = L"Windows.Internal.Gaming.SWDDeviceStatics";
	HSTRING_HEADER header;
	memset(&header, 0, sizeof(header));
	HSTRING string = NULL;
	if (SUCCEEDED(WindowsCreateStringReference(type_name, (UINT32)wcslen(type_name), &header, &string)))
	{
		const GUID IID_SWDeviceStatics = {
			0x5189313c, 0xfc43, 0x41b2, { 0x82, 0xcc, 0x27, 0x1a, 0x0e, 0xe2, 0x9e, 0x80 }
		};
		void* factory = NULL;
		if (FAILED(RoGetActivationFactory(string, &IID_SWDeviceStatics, &factory)))
		{
			RoInitialize(RO_INIT_MULTITHREADED);
		}
	}
}

static void WINAPI DuoController_DeviceChanged(_In_ HDEVQUERY query, _In_opt_ PVOID context, _In_ const DEV_QUERY_RESULT_ACTION_DATA* actionData)
{
	(void)query;
	(void)context;
	if (actionData->Action == DevQueryResultAdd)
	{
		DEVPROPERTY deviceProperty;
		deviceProperty.CompKey.Key = DEVPKEY_Device_SessionId;
		deviceProperty.CompKey.Store = DEVPROP_STORE_SYSTEM;
		deviceProperty.CompKey.LocaleName = NULL;
		deviceProperty.Type = DEVPROP_TYPE_UINT32;
		deviceProperty.BufferSize = sizeof(SessionId);
		deviceProperty.Buffer = (PBYTE)&SessionId;
		DevSetObjectProperties(DevObjectTypeDevice, actionData->Data.DeviceObject.pszObjectId, 1, &deviceProperty);
	}
}

DWORD GetForegroundWindowProcessId(DWORD timeoutMs)
{
	DWORD processId = 0;
	DWORD startTime = GetTickCount();
	do
	{
		HWND hwnd = GetForegroundWindow();
		if (hwnd != NULL)
		{
			GetWindowThreadProcessId(hwnd, &processId);
			break;
		}
		Sleep(10);
	} while ((GetTickCount64() - startTime) < timeoutMs);
	return processId;
}

static NTSTATUS UpdateGameInputDriverFocusState()
{
	NTSTATUS result = ERROR_PROC_NOT_FOUND;
	if (RtlPublishWnfStateData != NULL)
	{
		result = RtlPublishWnfStateData(WNF_SHEL_FOCUS_CHANGE, NULL, &ForegroundWindowProcessId, sizeof(ForegroundWindowProcessId), NULL);
	}
	return result;
}

static void CALLBACK DuoController_OutputReportReceived(void* controller, unsigned int outputReportType, void* outputReportBuffer, unsigned int outputReportBufferSize, void* context)
{
	(void)controller;
	InitializeWindowsRuntimeForCurrentThread();
	ForegroundWindowProcessId = GetForegroundWindowProcessId(500);
	DUO_CONTROLLER* duoController = (DUO_CONTROLLER*)context;
	DUO_CONTROLLER_FORCE_FEEDBACK_REPORT* outputReport = (DUO_CONTROLLER_FORCE_FEEDBACK_REPORT*)outputReportBuffer;
	if (outputReportType == SYNTHETIC_CONTROLLER_OUTPUT_REPORT_TYPE_CONTROLLER &&
		outputReportBuffer != NULL && outputReportBufferSize >= sizeof(DUO_CONTROLLER_FORCE_FEEDBACK_REPORT) &&
		duoController != NULL && duoController->VibrationReportCallback != NULL)
	{
		duoController->VibrationReportCallback(duoController, outputReport, duoController->VibrationReportCallbackContext);
	}
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
	BOOL driverInStore = FALSE;
	if (!SetupCopyOEMInfW(fullInfPath, NULL, SPOST_NONE, SP_COPY_NOOVERWRITE, NULL, 0, NULL, NULL) && GetLastError() == ERROR_FILE_EXISTS)
		driverInStore = TRUE;
	if (!driverInStore)
	{
		if (!DiInstallDriverW(NULL, fullInfPath, DIIRFLAG_FORCE_INF, &rebootRequired))
		{
			result = HRESULT_FROM_WIN32(GetLastError());
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
			SetupDiDestroyDeviceInfoList(hDevInfo);
			return result;
		}
		if (hidInstanceId != NULL && hidInstanceIdSize > 0)
		{
			hidInstanceId[0] = L'\0';
			DWORD pollStart = GetTickCount();
			while ((GetTickCount() - pollStart) < 5000)
			{
				WCHAR mutableId[256];
				wcscpy_s(mutableId, ARRAYSIZE(mutableId), instanceId);
				DEVINST rootDevInst;
				if (CM_Locate_DevNodeW(&rootDevInst, mutableId, CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS)
				{
					DEVINST childDevInst;
					if (CM_Get_Child(&childDevInst, rootDevInst, 0) == CR_SUCCESS)
					{
						WCHAR childId[256];
						ULONG len = ARRAYSIZE(childId);
						if (CM_Get_Device_IDW(childDevInst, childId, len, 0) == CR_SUCCESS)
						{
							if (wcsstr(childId, deviceIdSeed) != NULL)
							{
								wcscpy_s(hidInstanceId, hidInstanceIdSize, childId);
								break;
							}
						}
					}
				}
				Sleep(100);
			}
		}
		RegDeleteValueW(duoRegistryKey, valueName);
		RegCloseKey(duoRegistryKey);
	}
	SetupDiDestroyDeviceInfoList(hDevInfo);
	return result;
}

static HRESULT RemoveDuoControllerDevice(const WCHAR* instanceId, const WCHAR* hidInstanceId)
{
	if (!IsProcessElevated())
		return E_ACCESSDENIED;
	if (hidInstanceId != NULL && hidInstanceId[0] != L'\0')
	{
		HDEVINFO hHidInfo = SetupDiCreateDeviceInfoList(NULL, NULL);
		if (hHidInfo != INVALID_HANDLE_VALUE)
		{
			SP_DEVINFO_DATA hidDevInfoData;
			hidDevInfoData.cbSize = sizeof(hidDevInfoData);
			if (SetupDiOpenDeviceInfoW(hHidInfo, hidInstanceId, NULL, 0, &hidDevInfoData))
			{
				SP_REMOVEDEVICE_PARAMS removeParams;
				ZeroMemory(&removeParams, sizeof(removeParams));
				removeParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
				removeParams.ClassInstallHeader.InstallFunction = DIF_REMOVE;
				removeParams.Scope = DI_REMOVEDEVICE_GLOBAL;
				if (SetupDiSetClassInstallParamsW(hHidInfo, &hidDevInfoData,
					(PSP_CLASSINSTALL_HEADER)&removeParams, sizeof(removeParams)))
				{
					if (!SetupDiCallClassInstaller(DIF_REMOVE, hHidInfo, &hidDevInfoData))
					{
						SetupDiRemoveDevice(hHidInfo, &hidDevInfoData);
					}
				}
			}
			SetupDiDestroyDeviceInfoList(hHidInfo);
		}
	}
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
			if (SetupDiCallClassInstaller(DIF_REMOVE, hDevInfo, &devInfoData) || SetupDiRemoveDevice(hDevInfo, &devInfoData))
				result = S_OK;
			else
				result = HRESULT_FROM_WIN32(GetLastError());
		}
		else
		{
			result = HRESULT_FROM_WIN32(GetLastError());
		}
	}
	SetupDiDestroyDeviceInfoList(hDevInfo);
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
	DWORD startTime = GetTickCount();
	HANDLE hMapping = NULL;
	while (GetTickCount() - startTime < 1000)
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
	DWORD startTime = GetTickCount();
	HANDLE hMapping = NULL;
	while (GetTickCount() - startTime < 1000)
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
	inputMem[0] = DS_INPUT_REPORT_FULL;
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
	WCHAR seed[64];
	swprintf_s(seed, ARRAYSIZE(seed), L"VID_054C&PID_%04X&DUOCONTROLLER", pid);
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
#pragma warning(suppress:4366)
		DeleteCriticalSection(&controller->DsCs);
		return result;
	}
	result = DsConnectFfb(controller);
	if (FAILED(result))
	{
		DsDisconnectInput(controller);
#pragma warning(suppress:4366)
		DeleteCriticalSection(&controller->DsCs);
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
	return RemoveDuoControllerDevice(controller->DsInstanceId, controller->DsHidInstanceId);
}

static HRESULT SendDsReport(DUO_CONTROLLER* controller, DUO_CONTROLLER_INPUT_REPORT_DUALSENSE* inputReport)
{
#pragma warning(suppress:4366)
	EnterCriticalSection(&controller->DsCs);
	inputReport->SeqNo++;
	inputReport->DeviceTimeStamp = GetTickCount();
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
	DWORD startTime = GetTickCount();
	HANDLE hMapping = NULL;
	while (GetTickCount() - startTime < 1000)
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
	DWORD startTime = GetTickCount();
	HANDLE hMapping = NULL;
	while (GetTickCount() - startTime < 1000)
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
	inputMem[0] = DS4_INPUT_REPORT_FULL;
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
	HRESULT result = InstallDuoControllerDevice(L"Root\\VID_054C&PID_05C4", L"VID_054C&PID_05C4&DUOCONTROLLER", instanceId, ARRAYSIZE(instanceId), hidInstanceId, ARRAYSIZE(hidInstanceId));
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
#pragma warning(suppress:4366)
		DeleteCriticalSection(&controller->Ds4Cs);
		return result;
	}
	result = Ds4ConnectFfb(controller);
	if (FAILED(result))
	{
		Ds4DisconnectInput(controller);
#pragma warning(suppress:4366)
		DeleteCriticalSection(&controller->Ds4Cs);
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
	return RemoveDuoControllerDevice(controller->Ds4InstanceId, controller->Ds4HidInstanceId);
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

// ==================== Exported API ====================

HRESULT WINAPI DuoController_Initialize()
{
	HRESULT result = S_OK;
	InitializeWindowsRuntimeForCurrentThread();
	if (!Initialized)
	{
		HMODULE xboxgipsynthetic = NULL;
		if ((GetModuleHandleExW(0, L"xboxgipsynthetic.dll", &xboxgipsynthetic) && xboxgipsynthetic != NULL) || (xboxgipsynthetic = LoadLibraryExW(L"xboxgipsynthetic.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32)) != NULL)
		{
			HMODULE cfgmgr32 = NULL;
			if ((GetModuleHandleExW(0, L"cfgmgr32.dll", &cfgmgr32) && cfgmgr32 != NULL) || (cfgmgr32 = LoadLibraryExW(L"cfgmgr32.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32)) != NULL)
			{
				HMODULE ntdll = NULL;
				if ((GetModuleHandleExW(0, L"ntdll.dll", &ntdll) && ntdll != NULL) || (ntdll = LoadLibraryExW(L"ntdll.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32)) != NULL)
				{
					SyntheticController_CreateController = (SyntheticController_CreateController_t)GetProcAddress(xboxgipsynthetic, "SyntheticController_CreateController");
					SyntheticController_SetTargetProcess = (SyntheticController_SetTargetProcess_t)GetProcAddress(xboxgipsynthetic, "SyntheticController_SetTargetProcess");
					SyntheticController_RegisterReportCallback = (SyntheticController_RegisterReportCallback_t)GetProcAddress(xboxgipsynthetic, "SyntheticController_RegisterReportCallback");
					SyntheticController_Connect = (SyntheticController_Connect_t)GetProcAddress(xboxgipsynthetic, "SyntheticController_Connect");
					SyntheticController_SendReport = (SyntheticController_SendReport_t)GetProcAddress(xboxgipsynthetic, "SyntheticController_SendReport");
					SyntheticController_Disconnect = (SyntheticController_Disconnect_t)GetProcAddress(xboxgipsynthetic, "SyntheticController_Disconnect");
					SyntheticController_UnregisterReportCallback = (SyntheticController_UnregisterReportCallback_t)GetProcAddress(xboxgipsynthetic, "SyntheticController_UnregisterReportCallback");
					SyntheticController_RemoveController = (SyntheticController_RemoveController_t)GetProcAddress(xboxgipsynthetic, "SyntheticController_RemoveController");
					SyntheticController_GetDeviceId = (SyntheticController_GetDeviceId_t)GetProcAddress(xboxgipsynthetic, "SyntheticController_GetDeviceId");
					DevSetObjectProperties = (DevSetObjectProperties_t)GetProcAddress(cfgmgr32, "DevSetObjectProperties");
					RtlPublishWnfStateData = (RtlPublishWnfStateData_t)GetProcAddress(ntdll, "RtlPublishWnfStateData");
					if (SyntheticController_CreateController != NULL &&
						SyntheticController_SetTargetProcess != NULL &&
						SyntheticController_RegisterReportCallback &&
						SyntheticController_Connect != NULL &&
						SyntheticController_SendReport != NULL &&
						SyntheticController_Disconnect != NULL &&
						SyntheticController_UnregisterReportCallback != NULL &&
						SyntheticController_RemoveController != NULL &&
						SyntheticController_GetDeviceId != NULL &&
						DevSetObjectProperties != NULL)
					{
						Initialized = TRUE;
					}
					else
					{
						result = E_NOINTERFACE;
					}
					if (result != S_OK)
					{
						FreeLibrary(xboxgipsynthetic);
						SyntheticController_CreateController = NULL;
						SyntheticController_SetTargetProcess = NULL;
						SyntheticController_RegisterReportCallback = NULL;
						SyntheticController_Connect = NULL;
						SyntheticController_SendReport = NULL;
						SyntheticController_Disconnect = NULL;
						SyntheticController_UnregisterReportCallback = NULL;
						SyntheticController_RemoveController = NULL;
						SyntheticController_GetDeviceId = NULL;
					}
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
		else
		{
			result = HRESULT_FROM_WIN32(GetLastError());
		}
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
		HMODULE xboxgipsynthetic = NULL;
		if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, L"xboxgipsynthetic.dll", &xboxgipsynthetic) && xboxgipsynthetic != NULL)
			FreeLibrary(xboxgipsynthetic);
		HMODULE cfgmgr32 = NULL;
		if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, L"cfgmgr32.dll", &cfgmgr32) && cfgmgr32 != NULL)
			FreeLibrary(cfgmgr32);
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
					newController->SyntheticHandle = NULL;
					newController->DeviceQuery = NULL;
					ZeroMemory(&newController->LastXboxInputReport, sizeof(newController->LastXboxInputReport));
#pragma warning(suppress:4366)
					if ((result = SyntheticController_CreateController(SyntheticControllerTypeExtendedGamepad, &newController->SyntheticHandle)) == S_OK)
					{
						unsigned long long deviceId = 0;
						if (SyntheticController_GetDeviceId(newController->SyntheticHandle, &deviceId) == S_OK)
						{
							DWORD deviceIdSuffixBufferSize = 128;
							WCHAR* deviceIdSuffix = (WCHAR*)malloc(deviceIdSuffixBufferSize * sizeof(WCHAR));
							if (deviceIdSuffix != NULL)
							{
								swprintf_s(deviceIdSuffix, deviceIdSuffixBufferSize, L"&%016llX", deviceId);
								DEVPROP_FILTER_EXPRESSION ObjectFilter[] =
								{
									{ DEVPROP_OPERATOR_OR_OPEN },
									{ DEVPROP_OPERATOR_ENDS_WITH_IGNORE_CASE, { { DEVPKEY_Device_InstanceId, DEVPROP_STORE_SYSTEM, NULL }, DEVPROP_TYPE_STRING, ((ULONG)wcslen(deviceIdSuffix) + 1) * sizeof(WCHAR), (BYTE*)deviceIdSuffix } },
									{ DEVPROP_OPERATOR_ENDS_WITH_IGNORE_CASE, { { DEVPKEY_Device_Parent, DEVPROP_STORE_SYSTEM, NULL }, DEVPROP_TYPE_STRING, ((ULONG)wcslen(deviceIdSuffix) + 1) * sizeof(WCHAR), (BYTE*)deviceIdSuffix } },
									{ DEVPROP_OPERATOR_OR_CLOSE }
								};
#pragma warning(suppress:4366)
								if ((result = DevCreateObjectQuery(DevObjectTypeDevice, DevQueryFlagUpdateResults, 0, NULL, ARRAYSIZE(ObjectFilter), ObjectFilter, DuoController_DeviceChanged, NULL, &newController->DeviceQuery)) == S_OK)
								{
									if ((result = SyntheticController_RegisterReportCallback(newController->SyntheticHandle, SYNTHETIC_CONTROLLER_OUTPUT_REPORT_TYPE_CONTROLLER, DuoController_OutputReportReceived, newController)) == S_OK)
									{
										if ((result = SyntheticController_Connect(newController->SyntheticHandle)) == S_OK)
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
												result = E_OUTOFMEMORY;
											}
											if (result != S_OK)
												SyntheticController_Disconnect(newController->SyntheticHandle);
										}
										if (result != S_OK)
											SyntheticController_UnregisterReportCallback(newController->SyntheticHandle, SYNTHETIC_CONTROLLER_OUTPUT_REPORT_TYPE_CONTROLLER);
									}
									if (result != S_OK)
										DevCloseObjectQuery(newController->DeviceQuery);
								}
								free(deviceIdSuffix);
							}
						}
						if (result != S_OK)
							SyntheticController_RemoveController(newController->SyntheticHandle);
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
					SyntheticController_Disconnect(duoController->SyntheticHandle);
					SyntheticController_UnregisterReportCallback(duoController->SyntheticHandle, SYNTHETIC_CONTROLLER_OUTPUT_REPORT_TYPE_CONTROLLER);
					DevCloseObjectQuery(duoController->DeviceQuery);
					SyntheticController_RemoveController(duoController->SyntheticHandle);
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
				DUO_CONTROLLER_INPUT_REPORT_XBOX* xboxReport = (DUO_CONTROLLER_INPUT_REPORT_XBOX*)inputReport;
				if ((result = SyntheticController_SendReport(duoController->SyntheticHandle, SYNTHETIC_CONTROLLER_INPUT_REPORT_TYPE_EXTENDED_CONTROLLER, xboxReport, sizeof(DUO_CONTROLLER_INPUT_REPORT_XBOX))) == S_OK)
				{
					SYNTHETIC_CONTROLLER_VKEY_INPUT_REPORT guideButtonInputReport;
					memset(&guideButtonInputReport, 0, sizeof(guideButtonInputReport));
					guideButtonInputReport.State = xboxReport->Guide;
					SyntheticController_SendReport(duoController->SyntheticHandle, SYNTHETIC_CONTROLLER_INPUT_REPORT_TYPE_VKEY, &guideButtonInputReport, sizeof(guideButtonInputReport));
					duoController->LastXboxInputReport = *xboxReport;
				}
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
