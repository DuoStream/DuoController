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

	// For DualSense feature reports, return zero-filled buffer for all report IDs.
	// Real hardware returns calibration data; for a virtual device this is sufficient.
	UNREFERENCED_PARAMETER(QueueContext);

	if (packet.reportBufferLen > 0)
	{
		RtlZeroMemory(packet.reportBuffer, packet.reportBufferLen);
		if (packet.reportBufferLen >= sizeof(UCHAR))
		{
			packet.reportBuffer[0] = (UCHAR)packet.reportId;
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

		if (stringIndex != HID_DEVICE_STRING_INDEX)
		{
			status = STATUS_INVALID_PARAMETER;
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_IOCTL, 
				"GetString: unknown string index %d\n", 
				stringIndex);
			return status;
		}

		status = RequestCopyFromBuffer(Request, HID_DEVICE_STRING, sizeof(HID_DEVICE_STRING));
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