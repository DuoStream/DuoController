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
#include "Ioctl.tmh"

/// <summary>
/// Copies the specified number of bytes to the request's output memory buffer.
/// </summary>
/// <param name="Request">Handle to a framework request object.</param>
/// <param name="SourceBuffer">The buffer to copy data from.</param>
/// <param name="NumBytesToCopyFrom">The length, in bytes, of data to copy.</param>
/// <returns>NTSTATUS</returns>
NTSTATUS RequestCopyFromBuffer(_In_ WDFREQUEST Request, _In_ PVOID SourceBuffer, _When_(NumBytesToCopyFrom == 0, __drv_reportError(NumBytesToCopyFrom cannot be zero)) _In_ size_t NumBytesToCopyFrom)
{
	NTSTATUS status;
	WDFMEMORY memory;
	size_t outputBufferLength;

	status = WdfRequestRetrieveOutputMemory(Request, &memory);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"WdfRequestRetrieveOutputMemory failed %!STATUS!",
			status);
		return status;
	}

	WdfMemoryGetBuffer(memory, &outputBufferLength);
	if (outputBufferLength < NumBytesToCopyFrom)
	{
		status = STATUS_INVALID_BUFFER_SIZE;

		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, 
			"RequestCopyFromBuffer: buffer too small. Size %d, expect %d\n",
			(int)outputBufferLength, 
			(int)NumBytesToCopyFrom);
		return status;
	}

	status = WdfMemoryCopyFromBuffer(memory,
		0,
		SourceBuffer,
		NumBytesToCopyFrom);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"WdfMemoryCopyFromBuffer failed %!STATUS!",
			status);
		return status;
	}

	WdfRequestSetInformation(Request, NumBytesToCopyFrom);
	return status;
}

/// <summary>
/// Handles IOCTL_HID_READ_REPORT for the HID collection. Forwards the request
/// to a manual queue for deferred completion when data is available. If forwarding
/// fails, the caller must complete the request with an error code immediately.
/// </summary>
/// <param name="QueueContext">The object context associated with the queue.</param>
/// <param name="Request">Pointer to the request packet.</param>
/// <param name="CompleteRequest">Boolean output indicating whether the caller should complete the request.</param>
/// <returns>NTSTATUS</returns>
NTSTATUS ReadReport(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request, _Always_(_Out_) BOOLEAN* CompleteRequest)
{
	NTSTATUS status;
	PVOID pOutputBuffer = NULL;
	size_t outputBufferLength;

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IOCTL, "%!FUNC! Entry");

	status = WdfRequestRetrieveOutputBuffer(
		Request,
		sizeof(UCHAR),
		&pOutputBuffer,
		&outputBufferLength
	);

	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"WdfRequestRetrieveOutputBuffer failed with status %!STATUS!",
			status
		);

		*CompleteRequest = TRUE;
		return status;
	}

	// Forward the request to the manual queue
	status = WdfRequestForwardToIoQueue(
		Request,
		QueueContext->DeviceContext->ManualQueue);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"WdfRequestForwardToIoQueue failed with %!STATUS!",
			status);
		*CompleteRequest = TRUE;

	}
	else
	{
		*CompleteRequest = FALSE;
	}

	return status;
}

/// <summary>
/// Handles IOCTL_HID_WRITE_REPORT for the HID collection. Extracts the HID transfer
/// packet, validates the buffer size, stores the DualSense output report, and writes the
/// response to the shared memory output client.
/// </summary>
/// <param name="QueueContext">The object context associated with the queue.</param>
/// <param name="Request">Handle to a framework request object.</param>
/// <returns>NTSTATUS</returns>
NTSTATUS WriteReport(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request)
{
	NTSTATUS status;
	HID_XFER_PACKET packet;

	status = RequestGetHidXferPacket_ToWriteToDevice(
		Request,
		&packet);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"RequestGetHidXferPacket_ToWriteToDevice failed %!STATUS!",
			status);
		return status;
	}

	if (packet.reportBufferLen < sizeof(UCHAR))
	{
		status = STATUS_BUFFER_TOO_SMALL;
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"WriteReport: input buffer too small!");
		return status;
	}

	// Store the DualSense output report
	if (packet.reportBufferLen >= sizeof(DS_OUTPUT_REPORT))
	{
		RtlCopyMemory(
			&QueueContext->DeviceContext->DsOutputReport,
			packet.reportBuffer,
			sizeof(DS_OUTPUT_REPORT));
	}

	QueueContext->DeviceContext->ReportPacket = packet;

	WriteResponseToOutputClient(QueueContext);

	WdfRequestSetInformation(Request, packet.reportBufferLen);
	return status;
}

/// <summary>
/// Extracts a HID_XFER_PACKET from a WDF request for read-from-device operations.
/// Retrieves the report ID from the request's input buffer and the report buffer
/// from the request's output buffer.
/// </summary>
/// <param name="Request">Handle to a framework request object.</param>
/// <param name="Packet">Pointer to a HID_XFER_PACKET structure to fill.</param>
/// <returns>NTSTATUS</returns>
NTSTATUS RequestGetHidXferPacket_ToReadFromDevice(_In_ WDFREQUEST Request, _Out_ HID_XFER_PACKET* Packet)
{

	NTSTATUS status;
	WDFMEMORY inputMemory;
	WDFMEMORY outputMemory;
	size_t inputBufferLength;
	size_t outputBufferLength;
	PVOID inputBuffer;
	PVOID outputBuffer;

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IOCTL, "%!FUNC! Entry");

	// Get report Id from input buffer
	status = WdfRequestRetrieveInputMemory(Request, &inputMemory);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, "WdfRequestRetrieveInputMemory failed %!STATUS!", status);
		return status;
	}
	inputBuffer = WdfMemoryGetBuffer(inputMemory, &inputBufferLength);

	if (inputBufferLength < sizeof(UCHAR))
	{
		status = STATUS_INVALID_BUFFER_SIZE;
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"WdfRequestRetrieveInputMemory: input buffer. size %d, expect %d\n",
			(int)inputBufferLength,
			(int)sizeof(UCHAR));
		return status;
	}

	Packet->reportId = *(PUCHAR)inputBuffer;

	// Get report buffer from output buffer
	status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"WdfRequestRetrieveOutputMemory failed %!STATUS!",
			status);
		return status;
	}

	outputBuffer = WdfMemoryGetBuffer(outputMemory, &outputBufferLength);

	Packet->reportBuffer = (PUCHAR)outputBuffer;
	Packet->reportBufferLen = (ULONG)outputBufferLength;

	return status;
}

/// <summary>
/// Extracts a HID_XFER_PACKET from a WDF request for write-to-device operations.
/// Retrieves the report ID from the output buffer length (workaround for driver
/// read-access limitations) and the report buffer from the input buffer.
/// </summary>
/// <param name="Request">Handle to a framework request object.</param>
/// <param name="Packet">Pointer to a HID_XFER_PACKET structure to fill.</param>
/// <returns>NTSTATUS</returns>
NTSTATUS RequestGetHidXferPacket_ToWriteToDevice(_In_ WDFREQUEST Request, _Out_ HID_XFER_PACKET* Packet)
{
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IOCTL, "%!FUNC! Entry");

	// Driver need to read from the input buffer (which was written by App)
	//   Report Buffer: Input Buffer
	//   Report Id    : Output Buffer Length
	//
	// Note that the report id is not stored inside the output buffer, as the
	// driver has no read-access right to the output buffer, and trying to read
	// from the buffer will cause an access violation error.
	//
	// The workaround is to store the report id in the OutputBufferLength field,
	// to which the driver does have read-access right.
	//

	NTSTATUS status;
	WDFMEMORY inputMemory;
	WDFMEMORY outputMemory;
	size_t inputBufferLength;
	size_t outputBufferLength;
	PVOID inputBuffer;

	// Get report Id from output buffer length
	status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"WdfRequestRetrieveOutputMemory failed %!STATUS!",
			status);
		return status;
	}
	WdfMemoryGetBuffer(outputMemory, &outputBufferLength);
	Packet->reportId = (UCHAR)outputBufferLength;

	// Get report buffer from input buffer
	status = WdfRequestRetrieveInputMemory(Request, &inputMemory);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"WdfRequestRetrieveInputMemory failed %!STATUS!",
			status);
		return status;
	}
	inputBuffer = WdfMemoryGetBuffer(inputMemory, &inputBufferLength);

	Packet->reportBuffer = (PUCHAR)inputBuffer;
	Packet->reportBufferLen = (ULONG)inputBufferLength;

	return status;
}

// Feature Report 5 (0x05) - IMU calibration data
static const DS_FEATURE_IN_IMU_CALIBRATION G_DsFeatureInImuCalibration =
{
	.ReportID = DS_IMU_CALIBRATION_REPORT_ID,
	.GyroPitchBias = 4, // -8 on Otakian's controller, 4 on mine.
	.GyroYawBias = -2, // 5 on Otakian's controller, -2 on mine.
	.GyroRollBias = 2, // -3 on Otakian's controller, 2 on mine.
	.GyroPitchPlus = 8805, // 8823 on Otakian's controller, 8805 on mine.
	.GyroPitchMinus = -8803, // -8860 on Otakian's controller, -8803 on mine.
	.GyroYawPlus = 8856, // 8856 on both controllers.
	.GyroYawMinus = -8854, // -8848 on Otakian's controller, -8854 on mine.
	.GyroRollPlus = 8985, // 8883 on Otakian's controller, 8985 on mine.
	.GyroRollMinus = -8987, // -8881 on Otakian's controller, -8987 on mine.
	.GyroSpeedPlus = 540, // 540 on both controllers.
	.GyroSpeedMinus = 540, // 540 on both controllers.
	.AccelXPlus = 8412, // 8152 on Otakian's controller, 8412 on mine.
	.AccelXMinus = -7965, // -8254 on Otakian's controller, -7965 on mine.
	.AccelYPlus = 8084, // 7905 on Otakian's controller, 8084 on mine.
	.AccelYMinus = -8292, // -8506 on Otakian's controller, -8292 on mine.
	.AccelZPlus = 8168, // 8192 on Otakian's controller, 8168 on mine.
	.AccelZMinus = -8196, // -8204 on Otakian's controller, -8196 on mine.
	.Unknown = 5, // 2 on Otakian's controller, 5 on mine.
	.Padding = { 0, 0, 0 }
};

// Feature Report 9 (0x09) - Get Controller and Host MAC
static const DS_FEATURE_IN_BT_PAIRING_DATA G_DsFeatureInBtPairingData =
{
	.ReportID = DS_BT_PAIRING_DATA_REPORT_ID,
	.ClientMac = { 0x43, 0x7F, 0xF0, 0x49, 0x18, 0x10 },
	.Hard08 = 0x08,
	.Hard25 = 0x25,
	.Hard00 = 0x00,
	.HostMac = { 0xDD, 0xB8, 0x90, 0x5D, 0xD5, 0xE0 },
	.Padding = { 0x00, 0x00, 0x00, 0x00 }
};

// Feature Report 32 (0x20) - Get Controller Firmware Version
static const DS_FEATURE_IN_FW_VERSION G_DsFeatureInFwVersion =
{
	.ReportID = DS_FIRMWARE_VERSION_REPORT_ID,
	//.BuildDate = "Jul  4 2025",
	.BuildDate = { 'J', 'u', 'l', ' ', ' ', '4', ' ', '2', '0', '2', '5' },
	//.BuildTime = "10:10:32",
	.BuildTime = { '1', '0', ':', '1', '0', ':', '3', '2' },
	.FwType = 2,
	.SwSeries = 4,
	.HardwareInfo = 0x00000621, // Generation 0x06, Variation 0x21
	.FirmwareVersion = 0x0110002a, // 1.16.42
	.DeviceInfo = { 0x1, 0xa8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
	.UpdateVersion = 0x0630,
	.UpdateImageInfo = 0x0,
	.UpdateUnk = 0x0,
	.FwVersion1 = 0x0001003c, // 1.0.60
	.FwVersion2 = 0x0002000a, // 2.0.10
	.FwVersion3 = 0x00000006, // 0.0.6
	.Unknown = 0x0
};

/// <summary>
/// Handles IOCTL_HID_GET_FEATURE for the HID collection.
/// </summary>
/// <param name="QueueContext">The object context associated with the queue.</param>
/// <param name="Request">Pointer to the request packet.</param>
/// <returns>NTSTATUS</returns>
NTSTATUS GetFeature(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request)
{
	NTSTATUS status;
	HID_XFER_PACKET packet;
	ULONG reportSize;

	status = RequestGetHidXferPacket_ToReadFromDevice(
		Request,
		&packet);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"RequestGetHidXferPacket_ToReadFromDevice failed %!STATUS!",
			status);
		return status;
	}

	//
	// Since output buffer is for write only (no read allowed by UMDF in output
	// buffer), any read from output buffer would be reading garbage), so don't
	// let app embed custom control code in output buffer. The minidriver can
	// support multiple features using separate report ID instead of using
	// custom control code. Since this is targeted at report ID 1, we know it
	// is a request for getting attributes.
	//
	// While KMDF does not enforce the rule (disallow read from output buffer),
	// it is good practice to not do so.
	//

	reportSize = packet.reportBufferLen;

	if (packet.reportBufferLen < sizeof(UCHAR))
	{
		status = STATUS_BUFFER_TOO_SMALL;
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"GetFeature: input buffer too small!");
		return status;
	}

	UNREFERENCED_PARAMETER(QueueContext);

	if (packet.reportBufferLen > 0)
	{
		RtlZeroMemory(packet.reportBuffer, packet.reportBufferLen);

		switch (packet.reportId)
		{
		case DS_IMU_CALIBRATION_REPORT_ID:
			if (packet.reportBufferLen >= sizeof(G_DsFeatureInImuCalibration))
			{
				RtlCopyMemory(packet.reportBuffer, &G_DsFeatureInImuCalibration, sizeof(G_DsFeatureInImuCalibration));
			}
			else
			{
				status = STATUS_BUFFER_TOO_SMALL;
			}
			break;

		case DS_BT_PAIRING_DATA_REPORT_ID:
			if (packet.reportBufferLen >= sizeof(G_DsFeatureInBtPairingData))
			{
				RtlCopyMemory(packet.reportBuffer, &G_DsFeatureInBtPairingData, sizeof(G_DsFeatureInBtPairingData));
			}
			else
			{
				status = STATUS_BUFFER_TOO_SMALL;
			}
			break;

		case DS_FIRMWARE_VERSION_REPORT_ID:
			if (packet.reportBufferLen >= sizeof(G_DsFeatureInFwVersion))
			{
				RtlCopyMemory(packet.reportBuffer, &G_DsFeatureInFwVersion, sizeof(G_DsFeatureInFwVersion));
			}
			else
			{
				status = STATUS_BUFFER_TOO_SMALL;
			}
			break;

		default:
			// For all other report IDs, just set the report ID byte (zero-filled above)
			if (packet.reportBufferLen >= sizeof(UCHAR))
			{
				packet.reportBuffer[0] = (UCHAR)packet.reportId;
			}
			break;
		}
	}

	// Report how many bytes were copied
	QueueContext->DeviceContext->ReportPacket = packet;
	WdfRequestSetInformation(Request, reportSize);
	return status;
}

/// <summary>
/// Handles IOCTL_HID_SET_FEATURE for the HID collection.
/// For the control collection, processes user-defined control codes
/// for sideband communication.
/// </summary>
/// <param name="QueueContext">The object context associated with the queue.</param>
/// <param name="Request">Pointer to the request packet.</param>
/// <returns>NTSTATUS</returns>
NTSTATUS SetFeature(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request)
{
	NTSTATUS status;
	HID_XFER_PACKET packet;
	ULONG reportSize;

	status = RequestGetHidXferPacket_ToWriteToDevice(
		Request,
		&packet);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"RequestGetHidXferPacket_ToWriteToDevice failed %!STATUS!",
			status);
		return status;
	}

	// Before touching control code make sure buffer is big enough
	reportSize = packet.reportBufferLen;

	if (packet.reportBufferLen < sizeof(UCHAR))
	{
		status = STATUS_BUFFER_TOO_SMALL;
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"SetFeature: input buffer too small!");
		return status;
	}

	QueueContext->DeviceContext->ReportPacket = packet;

	WriteResponseToOutputClient(QueueContext);

	return status;
}

/// <summary>
/// Handles IOCTL_HID_GET_INPUT_REPORT for the HID collection.
/// </summary>
/// <param name="QueueContext">The object context associated with the queue.</param>
/// <param name="Request">Pointer to the request packet.</param>
/// <returns>NTSTATUS</returns>
NTSTATUS GetInputReport(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request)
{
	NTSTATUS status;
	HID_XFER_PACKET packet;
	ULONG reportSize;

	UNREFERENCED_PARAMETER(QueueContext);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IOCTL, "%!FUNC! Entry");

	status = RequestGetHidXferPacket_ToReadFromDevice(
		Request,
		&packet);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"RequestGetHidXferPacket_ToReadFromDevice failed %!STATUS!",
			status);
		return status;
	}

	reportSize = packet.reportBufferLen;

	if (packet.reportBufferLen < sizeof(UCHAR))
	{
		status = STATUS_BUFFER_TOO_SMALL;
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"GetInputReport: input buffer too small!");
		return status;
	}

	// Report how many bytes were copied
	WdfRequestSetInformation(Request, reportSize);
	return status;
}

/// <summary>
/// Handles IOCTL_HID_SET_OUTPUT_REPORT for the HID collection.
/// </summary>
/// <param name="QueueContext">The object context associated with the queue.</param>
/// <param name="Request">Pointer to the request packet.</param>
/// <returns>NTSTATUS</returns>
NTSTATUS SetOutputReport(_In_ PQUEUE_CONTEXT QueueContext, _In_ WDFREQUEST Request)
{
	NTSTATUS status;
	HID_XFER_PACKET packet;
	ULONG reportSize;

	status = RequestGetHidXferPacket_ToWriteToDevice(
		Request,
		&packet);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"RequestGetHidXferPacket_ToWriteToDevice failed %!STATUS!",
			status);
		return status;
	}

	// Before touching buffer make sure buffer is big enough
	reportSize = packet.reportBufferLen;

	if (packet.reportBufferLen < sizeof(UCHAR))
	{
		status = STATUS_BUFFER_TOO_SMALL;

		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"GetInputReport: input buffer too small!");
		return status;
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_IOCTL,
		"ReportId: %d, ReportBufferLength: %d\n",
		packet.reportId,
		reportSize);

	QueueContext->DeviceContext->ReportPacket = packet;

	// Report how many bytes were copied
	WdfRequestSetInformation(Request, reportSize);
	return status;
}

/// <summary>
/// Helper routine to decode IOCTL_HID_GET_INDEXED_STRING and IOCTL_HID_GET_STRING.
/// </summary>
/// <param name="Request">Pointer to the request packet.</param>
/// <param name="StringId">Receives the decoded string index.</param>
/// <param name="LanguageId">Receives the decoded language identifier.</param>
/// <returns>NTSTATUS</returns>
NTSTATUS GetStringId(_In_ WDFREQUEST Request, _Out_ ULONG* StringId, _Out_ ULONG* LanguageId)
{
	NTSTATUS status;
	ULONG inputValue;
	WDFMEMORY inputMemory;
	size_t inputBufferLength;
	PVOID inputBuffer;

	// mshidumdf.sys updates the IRP and passes the string id (or index) through
	// the input buffer correctly based on the IOCTL buffer type

	status = WdfRequestRetrieveInputMemory(Request, &inputMemory);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"WdfRequestRetrieveInputMemory %!STATUS!",
			status);
		return status;
	}
	inputBuffer = WdfMemoryGetBuffer(inputMemory, &inputBufferLength);

	// Make sure buffer is big enough
	if (inputBufferLength < sizeof(ULONG))
	{
		status = STATUS_INVALID_BUFFER_SIZE;
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
			"GetStringId: invalid input buffer. size %d, expect %d\n",
			(int)inputBufferLength,
			(int)sizeof(ULONG));
		return status;
	}

	inputValue = (*(PULONG)inputBuffer);

	// The least significant two bytes of the INT value contain the string id.
	* StringId = (inputValue & 0x0ffff);

	// The most significant two bytes of the INT value contain the language
	// ID (for example, a value of 1033 indicates English).
	*LanguageId = (inputValue >> 16);

	return status;
}

/// <summary>
/// Handles IOCTL_HID_GET_INDEXED_STRING for the HID collection.
/// </summary>
/// <param name="Request">Pointer to the request packet.</param>
/// <returns>NTSTATUS</returns>
NTSTATUS GetIndexedString(_In_ WDFREQUEST Request)
{
	NTSTATUS status;
	ULONG languageId, stringIndex;

	status = GetStringId(Request, &stringIndex, &languageId);

	// While we don't use the language id, some minidrivers might.
	UNREFERENCED_PARAMETER(languageId);

	if (NT_SUCCESS(status))
	{
		switch (stringIndex)
		{
		case HID_DEVICE_MANUFACTURER_STRING_INDEX:
			status = RequestCopyFromBuffer(Request, HID_DEVICE_MANUFACTURER_STRING, sizeof(HID_DEVICE_MANUFACTURER_STRING));
			break;
		case HID_DEVICE_PRODUCT_STRING_INDEX:
			status = RequestCopyFromBuffer(Request, HID_DEVICE_PRODUCT_STRING, sizeof(HID_DEVICE_PRODUCT_STRING));
			break;
		default:
			status = STATUS_INVALID_PARAMETER;
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL,
				"GetString: unknown string index %d\n",
				stringIndex);
		}
	}
	return status;
}

/// <summary>
/// Handles IOCTL_HID_GET_STRING for the HID collection.
/// </summary>
/// <param name="Request">Pointer to the request packet.</param>
/// <param name="DeviceContext">Pointer to the device context.</param>
/// <returns>NTSTATUS</returns>
NTSTATUS GetString(_In_ WDFREQUEST Request, _In_ PDEVICE_CONTEXT DeviceContext)
{
	NTSTATUS status;
	ULONG languageId, stringId;
	size_t stringSizeCb;
	PWSTR string;

	status = GetStringId(Request, &stringId, &languageId);

	// While we don't use the language id, some minidrivers might.
	UNREFERENCED_PARAMETER(languageId);

	if (!NT_SUCCESS(status))
	{
		return status;
	}

	switch (stringId)
	{
	case HID_STRING_ID_IMANUFACTURER:
		stringSizeCb = sizeof(HID_DEVICE_MANUFACTURER_STRING);
		string = HID_DEVICE_MANUFACTURER_STRING;
		break;
	case HID_STRING_ID_IPRODUCT:
		stringSizeCb = sizeof(HID_DEVICE_PRODUCT_STRING);
		string = HID_DEVICE_PRODUCT_STRING;
		break;
	case HID_STRING_ID_ISERIALNUMBER:
		stringSizeCb = wcslen(DeviceContext->SerialNumber) * sizeof(WCHAR) + sizeof(WCHAR);
		string = DeviceContext->SerialNumber;
		break;
	default:
		status = STATUS_INVALID_PARAMETER;
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, 
			"GetString: unkown string id %d\n", 
			stringId);
		return status;
	}

	status = RequestCopyFromBuffer(Request, string, stringSizeCb);
	return status;
}