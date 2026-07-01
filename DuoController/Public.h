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
// devcon install DuoController.inf "Root\VID_054C&PID_05C4"

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

// DualSense Edge shared memory constants
#define DS_INPUT_REPORT_PARTIAL 0x01
#define DS_INPUT_REPORT_FULL 0x02
#define DS_OUTPUT_REPORT_AVAILABLE 0x03

#define DS_REPORT_SIZE 64
#define DS_INPUT_REPORT_ID 0x01
#define DS_OUTPUT_REPORT_ID 0x02
#define DS_OUTPUT_REPORT_SIZE 47
#define DS_IMU_CALIBRATION_REPORT_ID 0x05
#define DS_BT_PAIRING_DATA_REPORT_ID 0x09
#define DS_FIRMWARE_VERSION_REPORT_ID 0x20

// DualShock 4 shared memory constants
#define DS4_INPUT_REPORT_PARTIAL 0x01
#define DS4_INPUT_REPORT_FULL 0x02
#define DS4_OUTPUT_REPORT_AVAILABLE 0x03

#define DS4_REPORT_SIZE 64
#define DS4_INPUT_REPORT_ID 0x01
#define DS4_OUTPUT_REPORT_ID 0x05
#define DS4_OUTPUT_REPORT_SIZE 31

// DualSense Edge button indices for the flags field (0-based)
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

// DualShock 4 button indices (0-based, mapping to HID Usage 1-14)
#define DS4_BUTTON_SQUARE    0
#define DS4_BUTTON_CROSS     1
#define DS4_BUTTON_CIRCLE    2
#define DS4_BUTTON_TRIANGLE  3
#define DS4_BUTTON_L1        4
#define DS4_BUTTON_R1        5
#define DS4_BUTTON_L2        6
#define DS4_BUTTON_R2        7
#define DS4_BUTTON_SHARE     8
#define DS4_BUTTON_OPTIONS   9
#define DS4_BUTTON_L3        10
#define DS4_BUTTON_R3        11
#define DS4_BUTTON_PS        12
#define DS4_BUTTON_TOUCHPAD  13

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

#define DS4_HAT_UP        0
#define DS4_HAT_UPRIGHT   1
#define DS4_HAT_RIGHT     2
#define DS4_HAT_DOWNRIGHT 3
#define DS4_HAT_DOWN      4
#define DS4_HAT_DOWNLEFT  5
#define DS4_HAT_LEFT      6
#define DS4_HAT_UPLEFT    7
#define DS4_HAT_NULL      8

// Touchpad dimensions
#define DS_TOUCHPAD_MAX_X 1919
#define DS_TOUCHPAD_MAX_Y 942

#define DS4_TOUCHPAD_MAX_X 1919
#define DS4_TOUCHPAD_MAX_Y 942

#include <pshpack1.h>

typedef struct _DS_OUTPUT_REPORT
{
	UCHAR ReportId;
	UCHAR Data[46];

} DS_OUTPUT_REPORT, *PDS_OUTPUT_REPORT;

typedef struct _DS4_OUTPUT_REPORT
{
	UCHAR ReportId;
	UCHAR Data[30];

} DS4_OUTPUT_REPORT, *PDS4_OUTPUT_REPORT;

typedef struct _DS_FEATURE_IN_IMU_CALIBRATION {
	UINT8 ReportID; // 0x05
	INT16 GyroPitchBias;
	INT16 GyroYawBias;
	INT16 GyroRollBias;
	INT16 GyroPitchPlus;
	INT16 GyroPitchMinus;
	INT16 GyroYawPlus;
	INT16 GyroYawMinus;
	INT16 GyroRollPlus;
	INT16 GyroRollMinus;
	INT16 GyroSpeedPlus;
	INT16 GyroSpeedMinus;
	INT16 AccelXPlus;
	INT16 AccelXMinus;
	INT16 AccelYPlus;
	INT16 AccelYMinus;
	INT16 AccelZPlus;
	INT16 AccelZMinus;
	INT16 Unknown;
	UINT8 Padding[3];
} DS_FEATURE_IN_IMU_CALIBRATION, *PDS_FEATURE_IN_IMU_CALIBRATION;

typedef struct _DS_FEATURE_IN_BT_PAIRING_DATA {
	UINT8 ReportID; // 0x09
	UINT8 ClientMac[6]; // Right to Left
	UINT8 Hard08;
	UINT8 Hard25;
	UINT8 Hard00;
	UINT8 HostMac[6]; // Right to Left
	UINT8 Padding[4]; // Size according to Linux driver
} DS_FEATURE_IN_BT_PAIRING_DATA, *PDS_FEATURE_IN_BT_PAIRING_DATA;

typedef struct _DS_FEATURE_IN_FW_VERSION {
	UINT8 ReportID; // 0x20
	char BuildDate[11]; // string
	char BuildTime[8]; // string
	UINT16 FwType;
	UINT16 SwSeries;
	UINT32 HardwareInfo; // 0x00FF0000 - Variation
	// 0x0000FF00 - Generation
	// 0x0000003F - Trial?
	// ^ Values tied to enumerations
	UINT32 FirmwareVersion; // 0xAABBCCCC AA.BB.CCCC
	char DeviceInfo[12];
	////
	UINT16 UpdateVersion;
	char UpdateImageInfo;
	char UpdateUnk;
	////
	UINT32 FwVersion1; // AKA SblFwVersion
	// 0xAABBCCCC AA.BB.CCCC
	// Ignored for FwType 0
	// HardwareVersion used for FwType 1
	// Unknown behavior if HardwareVersion < 0.1.38 for FwType 2 & 3
	// If HardwareVersion >= 0.1.38 for FwType 2 & 3
	UINT32 FwVersion2; // AKA VenomFwVersion
	UINT32 FwVersion3; // AKA SpiderDspFwVersion AKA BettyFwVer
	UINT32 Unknown; // May be Memory Control Unit for Non Volatile Storage
} DS_FEATURE_IN_FW_VERSION, *PDS_FEATURE_IN_FW_VERSION;

#include <poppack.h>

EXTERN_C_END
