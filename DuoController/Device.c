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

#include "Driver.h"
#include "Device.tmh"

HID_REPORT_DESCRIPTOR G_DefaultReportDescriptor[] = {
	0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
	0x09, 0x05,        // Usage (Game Pad)
	0xA1, 0x01,        // Collection (Application)
	0x85, DS_INPUT_REPORT_ID,
	0x09, 0x30,        //   Usage (X)
	0x09, 0x31,        //   Usage (Y)
	0x09, 0x32,        //   Usage (Z)
	0x09, 0x35,        //   Usage (Rz)
	0x09, 0x33,        //   Usage (Rx)
	0x09, 0x34,        //   Usage (Ry)
	0x15, 0x00,        //   Logical Minimum (0)
	0x26, 0xFF, 0x00,  //   Logical Maximum (255)
	0x75, 0x08,        //   Report Size (8)
	0x95, 0x06,        //   Report Count (6)
	0x81, 0x02,        //   Input (Data,Var,Abs)
	0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
	0x09, 0x20,        //   Usage (0x20)
	0x95, 0x01,        //   Report Count (1)
	0x81, 0x02,        //   Input (Data,Var,Abs)
	0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
	0x09, 0x39,        //   Usage (Hat switch)
	0x15, 0x00,        //   Logical Minimum (0)
	0x25, 0x07,        //   Logical Maximum (7)
	0x35, 0x00,        //   Physical Minimum (0)
	0x46, 0x3B, 0x01,  //   Physical Maximum (315)
	0x65, 0x14,        //   Unit (Eng Rot:Angular Pos)
	0x75, 0x04,        //   Report Size (4)
	0x95, 0x01,        //   Report Count (1)
	0x81, 0x42,        //   Input (Data,Var,Abs,Null)
	0x65, 0x00,        //   Unit (None)
	0x05, 0x09,        //   Usage Page (Button)
	0x19, 0x01,        //   Usage Minimum (0x01)
	0x29, 0x0F,        //   Usage Maximum (0x0F)
	0x15, 0x00,        //   Logical Minimum (0)
	0x25, 0x01,        //   Logical Maximum (1)
	0x75, 0x01,        //   Report Size (1)
	0x95, 0x0F,        //   Report Count (15)
	0x81, 0x02,        //   Input (Data,Var,Abs)
	0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
	0x09, 0x21,        //   Usage (0x21)
	0x95, 0x0D,        //   Report Count (13)
	0x81, 0x02,        //   Input (Data,Var,Abs)
	0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
	0x09, 0x22,        //   Usage (0x22)
	0x15, 0x00,        //   Logical Minimum (0)
	0x26, 0xFF, 0x00,  //   Logical Maximum (255)
	0x75, 0x08,        //   Report Size (8)
	0x95, 0x34,        //   Report Count (52)
	0x81, 0x02,        //   Input (Data,Var,Abs)
	0x85, DS_OUTPUT_REPORT_ID,
	0x09, 0x23,        //   Usage (0x23)
	0x95, 0x2F,        //   Report Count (47)
	0x91, 0x02,        //   Output (Data,Var,Abs)
	0x85, 0x05,        //   Report ID (5)
	0x09, 0x33,        //   Usage (0x33)
	0x95, 0x28,        //   Report Count (40)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x08,        //   Report ID (8)
	0x09, 0x34,        //   Usage (0x34)
	0x95, 0x2F,        //   Report Count (47)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x09,        //   Report ID (9)
	0x09, 0x24,        //   Usage (0x24)
	0x95, 0x13,        //   Report Count (19)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x0A,        //   Report ID (10)
	0x09, 0x25,        //   Usage (0x25)
	0x95, 0x1A,        //   Report Count (26)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x20,        //   Report ID (32)
	0x09, 0x26,        //   Usage (0x26)
	0x95, 0x3F,        //   Report Count (63)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x21,        //   Report ID (33)
	0x09, 0x27,        //   Usage (0x27)
	0x95, 0x04,        //   Report Count (4)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x22,        //   Report ID (34)
	0x09, 0x40,        //   Usage (0x40)
	0x95, 0x3F,        //   Report Count (63)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x80,        //   Report ID (128)
	0x09, 0x28,        //   Usage (0x28)
	0x95, 0x3F,        //   Report Count (63)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x81,        //   Report ID (129)
	0x09, 0x29,        //   Usage (0x29)
	0x95, 0x3F,        //   Report Count (63)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x82,        //   Report ID (130)
	0x09, 0x2A,        //   Usage (0x2A)
	0x95, 0x09,        //   Report Count (9)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x83,        //   Report ID (131)
	0x09, 0x2B,        //   Usage (0x2B)
	0x95, 0x3F,        //   Report Count (63)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x84,        //   Report ID (132)
	0x09, 0x2C,        //   Usage (0x2C)
	0x95, 0x3F,        //   Report Count (63)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x85,        //   Report ID (133)
	0x09, 0x2D,        //   Usage (0x2D)
	0x95, 0x02,        //   Report Count (2)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xA0,        //   Report ID (160)
	0x09, 0x2E,        //   Usage (0x2E)
	0x95, 0x01,        //   Report Count (1)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xE0,        //   Report ID (224)
	0x09, 0x2F,        //   Usage (0x2F)
	0x95, 0x3F,        //   Report Count (63)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xF0,        //   Report ID (240)
	0x09, 0x30,        //   Usage (0x30)
	0x95, 0x3F,        //   Report Count (63)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xF1,        //   Report ID (241)
	0x09, 0x31,        //   Usage (0x31)
	0x95, 0x3F,        //   Report Count (63)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xF2,        //   Report ID (242)
	0x09, 0x32,        //   Usage (0x32)
	0x95, 0x0F,        //   Report Count (15)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xF4,        //   Report ID (244)
	0x09, 0x35,        //   Usage (0x35)
	0x95, 0x3F,        //   Report Count (63)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xF5,        //   Report ID (245)
	0x09, 0x36,        //   Usage (0x36)
	0x95, 0x03,        //   Report Count (3)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0xC0               // End Collection
};

HID_DESCRIPTOR G_DefaultHidDescriptor = {
	0x09,   // length of HID descriptor
	0x21,   // descriptor type == HID  0x21
	0x0100, // hid spec release
	0x00,   // country code == Not Specified
	0x01,   // number of HID class descriptors
	{                                       //DescriptorList[0]
		0x22,                               //report descriptor type 0x22
		sizeof(G_DefaultReportDescriptor)   //total length of report descriptor
	}
};

/// <summary>
/// Cleanup callback invoked by the framework when the WDF device object is deleted.
/// Shuts down the shared memory server to release all inter-process communication resources.
/// </summary>
/// <param name="Device">Handle to a framework device object.</param>
VOID DuoControllerEvtDeviceCleanupCallback(_In_ WDFOBJECT Device)
{
	PDEVICE_CONTEXT devContext = DeviceGetContext((WDFDEVICE)Device);

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! Entry");

	ShutdownSharedMemoryServer(devContext);

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! Exit");
}

/// <summary>
/// Worker routine called to create a device and its software resources.
/// </summary>
/// <param name="DeviceInit">Pointer to an opaque init structure.</param>
/// <returns>NTSTATUS</returns>
NTSTATUS DuoControllerCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit)
{
	WDF_OBJECT_ATTRIBUTES deviceAttributes;
	PDEVICE_CONTEXT deviceContext;
	WDFDEVICE device;
	NTSTATUS status;

	// Mark ourselves as a filter, which also relinquishes power policy ownership
	WdfFdoInitSetFilter(DeviceInit);

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);
	deviceAttributes.EvtCleanupCallback = DuoControllerEvtDeviceCleanupCallback;

	status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

	if (NT_SUCCESS(status))
	{
		// Get a pointer to the device context structure
		deviceContext = DeviceGetContext(device);

		// Initialize the context
		deviceContext->Device = device;

		// Restrict device visibility to a specific session if needed
		{
			WDF_DEVICE_PROPERTY_DATA sessionPropertyData;
			WDF_DEVICE_PROPERTY_DATA_INIT(&sessionPropertyData, &DEVPKEY_Device_SessionId);
			ULONG sessionId = 0xFFFFFFFF;
			NTSTATUS sessionStatus = GetSessionIdFromRegistry(&sessionId);

			if (NT_SUCCESS(sessionStatus) && sessionId != 0xFFFFFFFF)
			{
				status = WdfDeviceAssignProperty(device, &sessionPropertyData,
					DEVPROP_TYPE_UINT32, sizeof(sessionId), &sessionId);
				if (NT_SUCCESS(status))
				{
					TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
						"Set SessionId property to %lu", sessionId);
				}
				else
				{
					TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
						"Failed to set SessionId property: %!STATUS!", status);
				}
			}
		}

		// Define device hardware Ids
		deviceContext->HidDeviceAttributes.VendorID = HID_DEVICE_VID;		
		deviceContext->HidDeviceAttributes.ProductID = HID_DEVICE_PID;
		deviceContext->HidDeviceAttributes.VersionNumber = HID_DEVICE_VERSION;

		// Retrieve the device instance ID for unique pipe naming
		{
			WDF_DEVICE_PROPERTY_DATA propertyData = { 0 };
			propertyData.Size = sizeof(propertyData);
			propertyData.PropertyKey = &DEVPKEY_Device_InstanceId;
			WDFMEMORY deviceInstanceIdMemory;
			DEVPROPTYPE propertyType;
			status = WdfDeviceAllocAndQueryPropertyEx(device, &propertyData, NonPagedPool, WDF_NO_OBJECT_ATTRIBUTES, (WDFMEMORY*)&deviceInstanceIdMemory, &propertyType);

			if (!NT_SUCCESS(status))
			{
				TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfDeviceGetDeviceProperty failed with %!STATUS!", status);
				return status;
			}

			WCHAR* deviceInstanceId = (WCHAR*)WdfMemoryGetBuffer(deviceInstanceIdMemory, NULL);

			if (deviceInstanceId == NULL)
			{
				TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfMemoryGetBuffer failed to get device instance ID buffer!");
				WdfObjectDelete(deviceInstanceIdMemory);
				return STATUS_UNSUCCESSFUL;
			}

			wcscpy_s(deviceContext->DeviceInstanceId, MAX_DEVICE_INSTANCE_ID_LEN, deviceInstanceId);

			// Generate a unique serial number from the device instance ID.
			// Instance ID format: ROOT\VID_054C&PID_0DF2&DUOCONTROLLER\0000
			// We use the suffix after the last backslash for uniqueness.
			WCHAR* serialStart = wcsrchr(deviceInstanceId, L'\\');
			if (serialStart != NULL)
			{
				serialStart++;
				wcscpy_s(deviceContext->SerialNumber, HID_DEVICE_SERIAL_NUMBER_MAX_LEN, serialStart);
			}
			else
			{
				wcscpy_s(deviceContext->SerialNumber, HID_DEVICE_SERIAL_NUMBER_MAX_LEN, deviceInstanceId);
			}

			WdfObjectDelete(deviceInstanceIdMemory);

			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Device instance ID: %ws", deviceContext->DeviceInstanceId);
		}

		// Initialize DualSense input report with default values
		SetDefaultDsReport(deviceContext->DsInputReport);

		// Initialize DualSense output report
		RtlZeroMemory(&deviceContext->DsOutputReport, sizeof(DS_OUTPUT_REPORT));

		// Use the hardcoded default HID descriptor and report descriptor
		deviceContext->HidDescriptor = G_DefaultHidDescriptor;
		deviceContext->ReportDescriptor = G_DefaultReportDescriptor;

		// Create a device interface so that applications can find and talk to us
		status = WdfDeviceCreateDeviceInterface(device, &GUID_DEVINTERFACE_DuoController, NULL);

		if (NT_SUCCESS(status))
		{
			// Initialize the I/O Package and any Queues
			status = DuoControllerQueueInitialize(device, &deviceContext->DefaultQueue);
			status = DuoControllerManualQueueInitialize(device, &deviceContext->ManualQueue);
		}

		if (NT_SUCCESS(status))
		{
			if (CreateSharedMemoryServer(device) > 0)
			{
				status = STATUS_UNSUCCESSFUL;
			}

			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Device VID_%04X&PID_%04X added.", deviceContext->HidDeviceAttributes.VendorID, deviceContext->HidDeviceAttributes.ProductID);
		}
	}

	return status;
}

/// <summary>
/// Reads the DuoControllerSessionId value from HKLM\SOFTWARE\Duo in the registry.
/// Used to restrict device visibility to a specific terminal session.
/// </summary>
/// <param name="SessionId">Output pointer that receives the session ID on success.</param>
/// <returns>NTSTATUS</returns>
NTSTATUS GetSessionIdFromRegistry(_Out_ PULONG SessionId)
{
	HKEY key = NULL;
	DWORD value = 0;
	DWORD valueSize = sizeof(value);
	DWORD valueType = 0;
	LONG result;

	if (SessionId == NULL)
	{
		return STATUS_INVALID_PARAMETER;
	}

	result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Duo", 0, KEY_READ, &key);

	if (result != ERROR_SUCCESS)
	{
		return HRESULT_FROM_WIN32(result);
	}

	result = RegQueryValueExW(key, L"DuoControllerSessionId", NULL, &valueType, (LPBYTE)&value, &valueSize);

	RegCloseKey(key);

	if (result != ERROR_SUCCESS)
	{
		return HRESULT_FROM_WIN32(result);
	}

	if (valueType != REG_DWORD || valueSize != sizeof(DWORD))
	{
		return STATUS_OBJECT_TYPE_MISMATCH;
	}

	*SessionId = value;

	return STATUS_SUCCESS;
}