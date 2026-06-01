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

#include "Public.h"

EXTERN_C_START

typedef UCHAR HID_REPORT_DESCRIPTOR, *PHID_REPORT_DESCRIPTOR;

#define HID_DEVICE_VID 0x054C
#define HID_DEVICE_PID 0x0DF2
#define HID_DEVICE_VERSION 0x0001

#define MAXIMUM_STRING_LENGTH (126 * sizeof(WCHAR))
#define MAX_DEVICE_INSTANCE_ID_LEN 200
#define HID_DEVICE_STRING L"DualSense Wireless Controller"
#define HID_DEVICE_MANUFACTURER_STRING L"Black-Seraph"
#define HID_DEVICE_PRODUCT_STRING L"DuoController"
#define HID_DEVICE_SERIAL_NUMBER_MAX_LEN 128
#define HID_DEVICE_STRING_INDEX 5

typedef struct _SHARED_MEMORY_SERVER_ATTRIBUTES
{
	WCHAR InputMemName[260];
	WCHAR OutputMemName[260];
	WCHAR InputEventName[260];
	WCHAR OutputEventName[260];
	HANDLE InputMappingHandle;
	HANDLE OutputMappingHandle;
	LPVOID InputView;
	LPVOID OutputView;
	HANDLE InputEvent;
	HANDLE OutputEvent;
	HANDLE InputThreadHandle;
	HANDLE StopEvent;
	PSECURITY_ATTRIBUTES SharedMemSecurityAttr;
} SHARED_MEMORY_SERVER_ATTRIBUTES, *PSHARED_MEMORY_SERVER_ATTRIBUTES;

// The device context performs the same job as a WDM device extension in the driver frameworks
typedef struct _DEVICE_CONTEXT
{
	WDFDEVICE Device;
	WDFQUEUE DefaultQueue;
	WDFQUEUE ManualQueue;
	HID_DESCRIPTOR HidDescriptor;
	PHID_REPORT_DESCRIPTOR ReportDescriptor;
	HID_DEVICE_ATTRIBUTES HidDeviceAttributes;
	WCHAR DeviceInstanceId[MAX_DEVICE_INSTANCE_ID_LEN];
	WCHAR SerialNumber[HID_DEVICE_SERIAL_NUMBER_MAX_LEN];
	HID_XFER_PACKET ReportPacket;
	UCHAR DsInputReport[DS_REPORT_SIZE];
	DS_OUTPUT_REPORT DsOutputReport;
	SHARED_MEMORY_SERVER_ATTRIBUTES SharedMemServerAttributes;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

// This macro will generate an inline function called DeviceGetContext
// which will be used to get a pointer to the device context memory
// in a type safe manner.
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

// Cleanup callback invoked when the device object is deleted
VOID DuoControllerEvtDeviceCleanupCallback(_In_ WDFOBJECT Device);

// Function to initialize the device and its callbacks
NTSTATUS DuoControllerCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit);

// Function to retrieves SessionId from registry (under HKR hardware key)
// Returns STATUS_NOT_FOUND when no explicit SessionId is configured.
NTSTATUS GetSessionIdFromRegistry(_Out_ PULONG SessionId);

EXTERN_C_END