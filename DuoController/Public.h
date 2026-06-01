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

EXTERN_C_START

// Use devcon to install a device: 
//
// devcon install DuoController.inf "Root\VID_054C&PID_0DF2"

// Define an Interface Guid so that apps can find the device and talk to it.
// {3d877443-4dda-4bab-a3e2-df607b30de4d}
DEFINE_GUID (GUID_DEVINTERFACE_DuoController, 0x3d877443,0x4dda,0x4bab,0xa3,0xe2,0xdf,0x60,0x7b,0x30,0xde,0x4d);

// Input client message header
typedef struct _MESSAGE_HEADER
{
	UCHAR MssageType;
	UCHAR MessageLength;

} MESSAGE_HEADER, *PMESSAGE_HEADER;

#define MESSAGE_HEADER_LEN sizeof(MESSAGE_HEADER)

#define DS_INPUT_REPORT_PARTIAL 0x01
#define DS_INPUT_REPORT_FULL 0x02
#define DS_OUTPUT_REPORT_AVAILABLE 0x03

#define DS_REPORT_SIZE 64
#define DS_INPUT_REPORT_ID 0x01
#define DS_OUTPUT_REPORT_ID 0x02
#define DS_OUTPUT_REPORT_SIZE 47

// DS button indices for the flags field (0-based)
#define DS_BUTTON_SQUARE    0
#define DS_BUTTON_CROSS     1
#define DS_BUTTON_CIRCLE    2
#define DS_BUTTON_TRIANGLE  3
#define DS_BUTTON_L1        4
#define DS_BUTTON_R1        5
#define DS_BUTTON_L2        6
#define DS_BUTTON_R2        7
#define DS_BUTTON_CREATE    8
#define DS_BUTTON_OPTIONS   9
#define DS_BUTTON_L3        10
#define DS_BUTTON_R3        11
#define DS_BUTTON_HOME      12
#define DS_BUTTON_PAD       13
#define DS_BUTTON_MUTE      14
#define DS_BUTTON_LEFT_FUNCTION  15
#define DS_BUTTON_RIGHT_FUNCTION 16
#define DS_BUTTON_LEFT_PADDLE    17
#define DS_BUTTON_RIGHT_PADDLE   18

// Hat switch values
#define DS_HAT_UP        0
#define DS_HAT_UPRIGHT   1
#define DS_HAT_RIGHT     2
#define DS_HAT_DOWNRIGHT 3
#define DS_HAT_DOWN      4
#define DS_HAT_DOWNLEFT  5
#define DS_HAT_LEFT      6
#define DS_HAT_UPLEFT    7
#define DS_HAT_NULL      8

// Touchpad dimensions
#define DS_TOUCHPAD_MAX_X 1919
#define DS_TOUCHPAD_MAX_Y 942

#include <pshpack1.h>

typedef struct _DS_OUTPUT_REPORT
{
	UCHAR ReportId;
	UCHAR Data[46];

} DS_OUTPUT_REPORT, *PDS_OUTPUT_REPORT;

#include <poppack.h>

EXTERN_C_END
