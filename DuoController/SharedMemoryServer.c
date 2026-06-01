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
#include "SharedMemoryServer.tmh"

// Size of the input shared memory region: header (2) + DualSense report (64)
#define INPUT_SHM_SIZE (MESSAGE_HEADER_LEN + DS_REPORT_SIZE)

// Size of the output shared memory region: DualSense output report (47)
#define OUTPUT_SHM_SIZE DS_OUTPUT_REPORT_SIZE

// Name format strings for shared memory and event objects
#define SHM_NAMESPACE_PREFIX L"Global\\Duo_"
#define SHM_NAME_FORMAT L"%s%s_%s"
#define SHM_INPUT_SUFFIX L"input"
#define SHM_OUTPUT_SUFFIX L"output"
#define SHM_INPUT_EVENT_SUFFIX L"input_event"
#define SHM_OUTPUT_EVENT_SUFFIX L"output_event"

#define SHM_NAME_BUFFER_SIZE 512

/// <summary>
/// Creates the shared memory server infrastructure for inter-process communication with the DuoController library.
/// Sets up input/output file mappings, synchronization events, security attributes, and starts the input processing thread.
/// </summary>
/// <param name="Params">Pointer to a WDFDEVICE handle.</param>
/// <returns>0 on success, 1 on failure.</returns>
DWORD CreateSharedMemoryServer(LPVOID Params)
{
	PDEVICE_CONTEXT devContext = DeviceGetContext(Params);

	PSHARED_MEMORY_SERVER_ATTRIBUTES attr = &devContext->SharedMemServerAttributes;

	// Sanitize the device instance ID for use in object names: replace backslashes with underscores
	WCHAR sanitizedInstanceId[MAX_DEVICE_INSTANCE_ID_LEN];
	size_t i;
	for (i = 0; i < MAX_DEVICE_INSTANCE_ID_LEN - 1 && devContext->DeviceInstanceId[i] != L'\0'; i++)
	{
		sanitizedInstanceId[i] = (devContext->DeviceInstanceId[i] == L'\\')
			? L'_'
			: devContext->DeviceInstanceId[i];
	}
	sanitizedInstanceId[i] = L'\0';

	// Create the security descriptor
	PSECURITY_ATTRIBUTES pSa = NULL;

	if (!CreateSharedMemorySecurity(&pSa))
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_PIPE,
			"CreateSharedMemorySecurity failed with %d\n", GetLastError()
		);
		FreeSharedMemorySecurity(pSa);
		pSa = NULL;
		return 1;
	}

	attr->SharedMemSecurityAttr = pSa;

	// Build input shared memory name
	int res = swprintf_s(attr->InputMemName,
		SHM_NAME_BUFFER_SIZE,
		SHM_NAME_FORMAT,
		SHM_NAMESPACE_PREFIX,
		sanitizedInstanceId,
		SHM_INPUT_SUFFIX);

	if (res <= 0)
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_PIPE,
			"Failed generating input shared memory name."
		);
		return 1;
	}

	// Build output shared memory name
	res = swprintf_s(attr->OutputMemName,
		SHM_NAME_BUFFER_SIZE,
		SHM_NAME_FORMAT,
		SHM_NAMESPACE_PREFIX,
		sanitizedInstanceId,
		SHM_OUTPUT_SUFFIX);

	if (res <= 0)
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_PIPE,
			"Failed generating output shared memory name."
		);
		return 1;
	}

	// Build input event name
	res = swprintf_s(attr->InputEventName,
		SHM_NAME_BUFFER_SIZE,
		SHM_NAME_FORMAT,
		SHM_NAMESPACE_PREFIX,
		sanitizedInstanceId,
		SHM_INPUT_EVENT_SUFFIX);

	if (res <= 0)
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_PIPE,
			"Failed generating input event name."
		);
		return 1;
	}

	// Build output event name
	res = swprintf_s(attr->OutputEventName,
		SHM_NAME_BUFFER_SIZE,
		SHM_NAME_FORMAT,
		SHM_NAMESPACE_PREFIX,
		sanitizedInstanceId,
		SHM_OUTPUT_EVENT_SUFFIX);

	if (res <= 0)
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_PIPE,
			"Failed generating output event name."
		);
		return 1;
	}

	// Create input file mapping
	attr->InputMappingHandle = CreateFileMappingW(
		INVALID_HANDLE_VALUE,
		pSa,
		PAGE_READWRITE,
		0,
		INPUT_SHM_SIZE,
		attr->InputMemName);

	if (attr->InputMappingHandle == NULL)
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_PIPE,
			"CreateFileMappingW for input failed with %d\n", GetLastError()
		);
		return 1;
	}

	// Map input view
	attr->InputView = MapViewOfFile(
		attr->InputMappingHandle,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		INPUT_SHM_SIZE);

	if (attr->InputView == NULL)
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_PIPE,
			"MapViewOfFile for input failed with %d\n", GetLastError()
		);
		CloseHandle(attr->InputMappingHandle);
		attr->InputMappingHandle = NULL;
		return 1;
	}

	// Create output file mapping
	attr->OutputMappingHandle = CreateFileMappingW(
		INVALID_HANDLE_VALUE,
		pSa,
		PAGE_READWRITE,
		0,
		OUTPUT_SHM_SIZE,
		attr->OutputMemName);

	if (attr->OutputMappingHandle == NULL)
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_PIPE,
			"CreateFileMappingW for output failed with %d\n", GetLastError()
		);
		UnmapViewOfFile(attr->InputView);
		attr->InputView = NULL;
		CloseHandle(attr->InputMappingHandle);
		attr->InputMappingHandle = NULL;
		return 1;
	}

	// Map output view
	attr->OutputView = MapViewOfFile(
		attr->OutputMappingHandle,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		OUTPUT_SHM_SIZE);

	if (attr->OutputView == NULL)
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_PIPE,
			"MapViewOfFile for output failed with %d\n", GetLastError()
		);
		CloseHandle(attr->OutputMappingHandle);
		attr->OutputMappingHandle = NULL;
		UnmapViewOfFile(attr->InputView);
		attr->InputView = NULL;
		CloseHandle(attr->InputMappingHandle);
		attr->InputMappingHandle = NULL;
		return 1;
	}

	// Create input event (auto-reset, initially non-signaled)
	// DuoController sets this event after writing input data
	attr->InputEvent = CreateEventW(pSa, FALSE, FALSE, attr->InputEventName);
	if (attr->InputEvent == NULL)
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_PIPE,
			"CreateEventW for input failed with %d\n", GetLastError()
		);
		UnmapViewOfFile(attr->OutputView);
		attr->OutputView = NULL;
		CloseHandle(attr->OutputMappingHandle);
		attr->OutputMappingHandle = NULL;
		UnmapViewOfFile(attr->InputView);
		attr->InputView = NULL;
		CloseHandle(attr->InputMappingHandle);
		attr->InputMappingHandle = NULL;
		return 1;
	}

	// Create output event (auto-reset, initially non-signaled)
	// DuoController sets this event after writing output (FFB) data
	attr->OutputEvent = CreateEventW(pSa, FALSE, FALSE, attr->OutputEventName);
	if (attr->OutputEvent == NULL)
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_PIPE,
			"CreateEventW for output failed with %d\n", GetLastError()
		);
		CloseHandle(attr->InputEvent);
		attr->InputEvent = NULL;
		UnmapViewOfFile(attr->OutputView);
		attr->OutputView = NULL;
		CloseHandle(attr->OutputMappingHandle);
		attr->OutputMappingHandle = NULL;
		UnmapViewOfFile(attr->InputView);
		attr->InputView = NULL;
		CloseHandle(attr->InputMappingHandle);
		attr->InputMappingHandle = NULL;
		return 1;
	}

	// Create stop event (manual-reset)
	attr->StopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
	if (attr->StopEvent == NULL)
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_PIPE,
			"CreateEventW for stop failed with %d\n", GetLastError()
		);
		CloseHandle(attr->OutputEvent);
		attr->OutputEvent = NULL;
		CloseHandle(attr->InputEvent);
		attr->InputEvent = NULL;
		UnmapViewOfFile(attr->OutputView);
		attr->OutputView = NULL;
		CloseHandle(attr->OutputMappingHandle);
		attr->OutputMappingHandle = NULL;
		UnmapViewOfFile(attr->InputView);
		attr->InputView = NULL;
		CloseHandle(attr->InputMappingHandle);
		attr->InputMappingHandle = NULL;
		return 1;
	}

	// Start the input shared memory thread
	attr->InputThreadHandle = CreateThread(
		NULL,
		0,
		InputSharedMemoryThread,
		Params,
		0,
		NULL);

	if (attr->InputThreadHandle == NULL)
	{
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_PIPE,
			"CreateThread for InputSharedMemoryThread failed with %d\n", GetLastError()
		);
		CloseHandle(attr->StopEvent);
		attr->StopEvent = NULL;
		CloseHandle(attr->OutputEvent);
		attr->OutputEvent = NULL;
		CloseHandle(attr->InputEvent);
		attr->InputEvent = NULL;
		UnmapViewOfFile(attr->OutputView);
		attr->OutputView = NULL;
		CloseHandle(attr->OutputMappingHandle);
		attr->OutputMappingHandle = NULL;
		UnmapViewOfFile(attr->InputView);
		attr->InputView = NULL;
		CloseHandle(attr->InputMappingHandle);
		attr->InputMappingHandle = NULL;
		return 1;
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_PIPE,
		"Shared memory server created. Input: %ws, Output: %ws\n",
		attr->InputMemName,
		attr->OutputMemName);

	return 0;
}

/// <summary>
/// Worker thread that waits for input events from the DuoController library via
/// shared memory. When data arrives, it reads the message, updates the DualSense input
/// report, and completes the pending read request from the manual queue.
/// </summary>
/// <param name="Params">Pointer to a WDFDEVICE handle.</param>
/// <returns>Thread exit code.</returns>
DWORD WINAPI InputSharedMemoryThread(LPVOID Params)
{
	PDEVICE_CONTEXT devContext = DeviceGetContext(Params);
	PSHARED_MEMORY_SERVER_ATTRIBUTES attr = &devContext->SharedMemServerAttributes;
	PQUEUE_CONTEXT queueContext = QueueGetContext(devContext->ManualQueue);

	// Wait handles: [0] = stop event, [1] = input event
	HANDLE handles[2];
	handles[0] = attr->StopEvent;
	handles[1] = attr->InputEvent;

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_PIPE,
		"Input shared memory thread started, waiting for input events.\n");

	while (1)
	{
		DWORD waitResult = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

		if (waitResult == WAIT_OBJECT_0)
		{
			// Stop event was signaled — shutdown
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_PIPE,
				"Input shared memory thread stopping.\n");
			break;
		}

		if (waitResult == WAIT_OBJECT_0 + 1)
		{
			// Input event was signaled — new data from DuoController

			// Double-check stop event (both could be signaled simultaneously)
			if (WaitForSingleObject(attr->StopEvent, 0) == WAIT_OBJECT_0)
			{
				break;
			}

			// Read the message from shared memory
			UCHAR buffer[BUFFER_SIZE];
			RtlZeroMemory(buffer, sizeof(buffer));
			RtlCopyMemory(buffer, attr->InputView,
				sizeof(buffer) < INPUT_SHM_SIZE ? sizeof(buffer) : INPUT_SHM_SIZE);

			switch (buffer[0])
			{
			case DS_INPUT_REPORT_FULL:
			{
				if (buffer[1] != DS_REPORT_SIZE)
				{
					TraceEvents(TRACE_LEVEL_ERROR, TRACE_PIPE,
						"DS_INPUT_REPORT has incorrect size. Expected: %d, Received: %d\n",
						DS_REPORT_SIZE,
						buffer[1]
					);
					break;
				}

				RtlCopyMemory(queueContext->DeviceContext->DsInputReport,
					&buffer[MESSAGE_HEADER_LEN],
					DS_REPORT_SIZE);

				CompleteReadRequest(devContext, DS_INPUT_REPORT_FULL);
				break;
			}
			default:
			{
				TraceEvents(TRACE_LEVEL_ERROR, TRACE_PIPE,
					"Unknown message type: %d\n", buffer[0]
				);
				break;
			}
			}
		}
	}

	return 0;
}

/// <summary>
/// Writes the current output report (e.g., force feedback data) to the shared memory
/// output view and signals the output event to notify the DuoController library.
/// </summary>
/// <param name="queueContext">The queue context containing the device context and report data.</param>
VOID WriteResponseToOutputClient(PQUEUE_CONTEXT queueContext)
{
	PSHARED_MEMORY_SERVER_ATTRIBUTES attr =
		&queueContext->DeviceContext->SharedMemServerAttributes;

	if (attr->OutputView == NULL ||
		WaitForSingleObject(attr->StopEvent, 0) == WAIT_OBJECT_0)
	{
		return;
	}

	// Copy the output report to shared memory
	RtlCopyMemory(
		attr->OutputView,
		queueContext->DeviceContext->ReportPacket.reportBuffer,
		queueContext->DeviceContext->ReportPacket.reportBufferLen < OUTPUT_SHM_SIZE
			? queueContext->DeviceContext->ReportPacket.reportBufferLen
			: OUTPUT_SHM_SIZE);

	// Signal the output event to notify DuoController
	SetEvent(attr->OutputEvent);
}

/// <summary>
/// Gracefully shuts down the shared memory server. Signals the stop and input events
/// to wake the input thread, waits for it to exit, then closes all mapping handles,
/// unmaps views, closes event handles, and frees the security attributes.
/// </summary>
/// <param name="devContext">The device context containing shared memory attributes.</param>
VOID ShutdownSharedMemoryServer(PDEVICE_CONTEXT devContext)
{
	PSHARED_MEMORY_SERVER_ATTRIBUTES attr = &devContext->SharedMemServerAttributes;

	// Signal stop event
	if (attr->StopEvent != NULL)
	{
		SetEvent(attr->StopEvent);
	}

	// Also signal the input event to wake the input thread
	if (attr->InputEvent != NULL)
	{
		SetEvent(attr->InputEvent);
	}

	// Wait for the input thread to exit
	if (attr->InputThreadHandle != NULL)
	{
		WaitForSingleObject(attr->InputThreadHandle, 5000);
		CloseHandle(attr->InputThreadHandle);
		attr->InputThreadHandle = NULL;
	}

	// Unmap views
	if (attr->InputView != NULL)
	{
		UnmapViewOfFile(attr->InputView);
		attr->InputView = NULL;
	}

	if (attr->OutputView != NULL)
	{
		UnmapViewOfFile(attr->OutputView);
		attr->OutputView = NULL;
	}

	// Close mapping handles
	if (attr->InputMappingHandle != NULL)
	{
		CloseHandle(attr->InputMappingHandle);
		attr->InputMappingHandle = NULL;
	}

	if (attr->OutputMappingHandle != NULL)
	{
		CloseHandle(attr->OutputMappingHandle);
		attr->OutputMappingHandle = NULL;
	}

	// Close event handles
	if (attr->InputEvent != NULL)
	{
		CloseHandle(attr->InputEvent);
		attr->InputEvent = NULL;
	}

	if (attr->OutputEvent != NULL)
	{
		CloseHandle(attr->OutputEvent);
		attr->OutputEvent = NULL;
	}

	if (attr->StopEvent != NULL)
	{
		CloseHandle(attr->StopEvent);
		attr->StopEvent = NULL;
	}

	// Free security attributes
	if (attr->SharedMemSecurityAttr != NULL)
	{
		FreeSharedMemorySecurity(attr->SharedMemSecurityAttr);
		attr->SharedMemSecurityAttr = NULL;
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_PIPE,
		"Shared memory server shut down.\n");
}

/// <summary>
/// Retrieves the next pending read request from the manual queue, copies the current
/// DualSense input report into the request's output buffer, and completes the request.
/// </summary>
/// <param name="devContext">The device context containing the DualSense input report.</param>
/// <param name="ReportId">The report type identifier (currently unused).</param>
VOID CompleteReadRequest(PDEVICE_CONTEXT devContext, UCHAR ReportId)
{
	(void)ReportId;

	NTSTATUS status = STATUS_UNSUCCESSFUL;
	WDFREQUEST reqRead = NULL;
	PVOID pReadReport = NULL;
	size_t bytesReturned = 0;

	status = WdfIoQueueRetrieveNextRequest(
		devContext->ManualQueue,
		&reqRead);

	if (!NT_SUCCESS(status))
	{
		if (status == STATUS_NO_MORE_ENTRIES)
		{
			return;
		}

		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_PIPE,
			"WdfIoQueueRetrieveNextRequest failed with status %!STATUS!",
			status);

		goto errorExit;
	}

	status = WdfRequestRetrieveOutputBuffer(
		reqRead,
		sizeof(UCHAR),
		&pReadReport,
		&bytesReturned);

	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_PIPE,
			"WdfRequestRetrieveOutputBuffer failed with status %!STATUS!",
			status);

		goto errorExit;
	}

	if (bytesReturned > 1)
	{
		RtlCopyMemory(pReadReport, devContext->DsInputReport,
			bytesReturned < DS_REPORT_SIZE ? bytesReturned : DS_REPORT_SIZE);
	}

	WdfRequestCompleteWithInformation(reqRead, status, bytesReturned);
	return;

errorExit:
	if (reqRead != NULL)
	{
		WdfRequestComplete(reqRead, status);
	}
}

/// <summary>
/// Creates a SECURITY_ATTRIBUTES structure with a security descriptor that grants
/// read/write access to Authenticated Users and full control to Administrators.
/// </summary>
/// <param name="ppSa">Output pointer that receives the allocated SECURITY_ATTRIBUTES on success.</param>
/// <returns>TRUE on success, FALSE on failure.</returns>
BOOL CreateSharedMemorySecurity(PSECURITY_ATTRIBUTES* ppSa)
{
	BOOL fSucceeded = TRUE;
	DWORD dwError = ERROR_SUCCESS;

	PSECURITY_DESCRIPTOR pSd = NULL;
	PSECURITY_ATTRIBUTES pSa = NULL;

	// Define the SDDL for the security descriptor.
	// Authenticated Users: read/write
	// Administrators: full control
	PCWSTR szSDDL = L"D:"
		L"(A;OICI;GRGW;;;AU)"
		L"(A;OICI;GA;;;BA)";

	if (!ConvertStringSecurityDescriptorToSecurityDescriptor(szSDDL,
		SDDL_REVISION_1, &pSd, NULL))
	{
		fSucceeded = FALSE;
		dwError = GetLastError();
		goto Cleanup;
	}

	pSa = (PSECURITY_ATTRIBUTES)HeapAlloc(GetProcessHeap(), 0, sizeof(*pSa));
	if (pSa == NULL)
	{
		fSucceeded = FALSE;
		dwError = GetLastError();
		goto Cleanup;
	}

	pSa->nLength = sizeof(*pSa);
	pSa->lpSecurityDescriptor = pSd;
	pSa->bInheritHandle = FALSE;

	*ppSa = pSa;

Cleanup:
	if (!fSucceeded)
	{
		if (pSd)
		{
			LocalFree(pSd);
			pSd = NULL;
		}
		if (pSa)
		{
			LocalFree(pSa);
			pSa = NULL;
		}
		SetLastError(dwError);
	}

	return fSucceeded;
}

/// <summary>
/// Frees the security descriptor and SECURITY_ATTRIBUTES structure allocated by
/// CreateSharedMemorySecurity.
/// </summary>
/// <param name="pSa">Pointer to the SECURITY_ATTRIBUTES to free.</param>
VOID FreeSharedMemorySecurity(PSECURITY_ATTRIBUTES pSa)
{
	if (pSa)
	{
		if (pSa->lpSecurityDescriptor)
		{
			LocalFree(pSa->lpSecurityDescriptor);
		}
		LocalFree(pSa);
	}
}