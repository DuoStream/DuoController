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

// DualSense Edge HID report descriptor (PID 0x0DF2)
HID_REPORT_DESCRIPTOR G_DsReportDescriptor[] = {
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

HID_DESCRIPTOR G_DsHidDescriptor = {
	0x09,   // length of HID descriptor
	0x21,   // descriptor type == HID  0x21
	0x0100, // hid spec release
	0x00,   // country code == Not Specified
	0x01,   // number of HID class descriptors
	{                                       //DescriptorList[0]
		0x22,                               //report descriptor type 0x22
		sizeof(G_DsReportDescriptor)        //total length of report descriptor
	}
};

// DualShock 4 HID report descriptor (PID 0x05C4)
HID_REPORT_DESCRIPTOR G_Ds4ReportDescriptor[] = {
	0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
	0x09, 0x05,        // Usage (Game Pad)
	0xA1, 0x01,        // Collection (Application)
	0x85, DS4_INPUT_REPORT_ID,
	0x09, 0x30,        //   Usage (X)
	0x09, 0x31,        //   Usage (Y)
	0x09, 0x32,        //   Usage (Z)
	0x09, 0x35,        //   Usage (Rz)
	0x15, 0x00,        //   Logical Minimum (0)
	0x26, 0xFF, 0x00,  //   Logical Maximum (255)
	0x75, 0x08,        //   Report Size (8)
	0x95, 0x04,        //   Report Count (4)
	0x81, 0x02,        //   Input (Data,Var,Abs)
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
	0x29, 0x0E,        //   Usage Maximum (0x0E)
	0x15, 0x00,        //   Logical Minimum (0)
	0x25, 0x01,        //   Logical Maximum (1)
	0x75, 0x01,        //   Report Size (1)
	0x95, 0x0E,        //   Report Count (14)
	0x81, 0x02,        //   Input (Data,Var,Abs)
	0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
	0x09, 0x20,        //   Usage (0x20)
	0x75, 0x06,        //   Report Size (6)
	0x95, 0x01,        //   Report Count (1)
	0x15, 0x00,        //   Logical Minimum (0)
	0x25, 0x7F,        //   Logical Maximum (127)
	0x81, 0x02,        //   Input (Data,Var,Abs)
	0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
	0x09, 0x33,        //   Usage (Rx)
	0x09, 0x34,        //   Usage (Ry)
	0x15, 0x00,        //   Logical Minimum (0)
	0x26, 0xFF, 0x00,  //   Logical Maximum (255)
	0x75, 0x08,        //   Report Size (8)
	0x95, 0x02,        //   Report Count (2)
	0x81, 0x02,        //   Input (Data,Var,Abs)
	0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
	0x09, 0x21,        //   Usage (0x21)
	0x95, 0x36,        //   Report Count (54)
	0x81, 0x02,        //   Input (Data,Var,Abs)
	0x85, DS4_OUTPUT_REPORT_ID,
	0x09, 0x22,        //   Usage (0x22)
	0x95, 0x1F,        //   Report Count (31)
	0x91, 0x02,        //   Output (Data,Var,Abs)
	0x85, 0x04,        //   Report ID (4)
	0x09, 0x23,        //   Usage (0x23)
	0x95, 0x24,        //   Report Count (36)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x02,        //   Report ID (2)
	0x09, 0x24,        //   Usage (0x24)
	0x95, 0x24,        //   Report Count (36)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x08,        //   Report ID (8)
	0x09, 0x25,        //   Usage (0x25)
	0x95, 0x03,        //   Report Count (3)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x10,        //   Report ID (16)
	0x09, 0x26,        //   Usage (0x26)
	0x95, 0x04,        //   Report Count (4)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x11,        //   Report ID (17)
	0x09, 0x27,        //   Usage (0x27)
	0x95, 0x02,        //   Report Count (2)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x12,        //   Report ID (18)
	0x06, 0x02, 0xFF,  //   Usage Page (Vendor Defined 0xFF02)
	0x09, 0x21,        //   Usage (0x21)
	0x95, 0x0F,        //   Report Count (15)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x13,        //   Report ID (19)
	0x09, 0x22,        //   Usage (0x22)
	0x95, 0x16,        //   Report Count (22)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x14,        //   Report ID (20)
	0x06, 0x05, 0xFF,  //   Usage Page (Vendor Defined 0xFF05)
	0x09, 0x20,        //   Usage (0x20)
	0x95, 0x10,        //   Report Count (16)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x15,        //   Report ID (21)
	0x09, 0x21,        //   Usage (0x21)
	0x95, 0x2C,        //   Report Count (44)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x06, 0x80, 0xFF,  //   Usage Page (Vendor Defined 0xFF80)
	0x85, 0x80,        //   Report ID (128)
	0x09, 0x20,        //   Usage (0x20)
	0x95, 0x06,        //   Report Count (6)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x81,        //   Report ID (129)
	0x09, 0x21,        //   Usage (0x21)
	0x95, 0x06,        //   Report Count (6)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x82,        //   Report ID (130)
	0x09, 0x22,        //   Usage (0x22)
	0x95, 0x05,        //   Report Count (5)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x83,        //   Report ID (131)
	0x09, 0x23,        //   Usage (0x23)
	0x95, 0x01,        //   Report Count (1)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x84,        //   Report ID (132)
	0x09, 0x24,        //   Usage (0x24)
	0x95, 0x04,        //   Report Count (4)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x85,        //   Report ID (133)
	0x09, 0x25,        //   Usage (0x25)
	0x95, 0x06,        //   Report Count (6)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x86,        //   Report ID (134)
	0x09, 0x26,        //   Usage (0x26)
	0x95, 0x06,        //   Report Count (6)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x87,        //   Report ID (135)
	0x09, 0x27,        //   Usage (0x27)
	0x95, 0x23,        //   Report Count (35)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x88,        //   Report ID (136)
	0x09, 0x28,        //   Usage (0x28)
	0x95, 0x22,        //   Report Count (34)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x89,        //   Report ID (137)
	0x09, 0x29,        //   Usage (0x29)
	0x95, 0x02,        //   Report Count (2)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x90,        //   Report ID (144)
	0x09, 0x30,        //   Usage (0x30)
	0x95, 0x05,        //   Report Count (5)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x91,        //   Report ID (145)
	0x09, 0x31,        //   Usage (0x31)
	0x95, 0x03,        //   Report Count (3)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x92,        //   Report ID (146)
	0x09, 0x32,        //   Usage (0x32)
	0x95, 0x03,        //   Report Count (3)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0x93,        //   Report ID (147)
	0x09, 0x33,        //   Usage (0x33)
	0x95, 0x0C,        //   Report Count (12)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xA0,        //   Report ID (160)
	0x09, 0x40,        //   Usage (0x40)
	0x95, 0x06,        //   Report Count (6)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xA1,        //   Report ID (161)
	0x09, 0x41,        //   Usage (0x41)
	0x95, 0x01,        //   Report Count (1)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xA2,        //   Report ID (162)
	0x09, 0x42,        //   Usage (0x42)
	0x95, 0x01,        //   Report Count (1)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xA3,        //   Report ID (163)
	0x09, 0x43,        //   Usage (0x43)
	0x95, 0x30,        //   Report Count (48)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xA4,        //   Report ID (164)
	0x09, 0x44,        //   Usage (0x44)
	0x95, 0x0D,        //   Report Count (13)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xA5,        //   Report ID (165)
	0x09, 0x45,        //   Usage (0x45)
	0x95, 0x15,        //   Report Count (21)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xA6,        //   Report ID (166)
	0x09, 0x46,        //   Usage (0x46)
	0x95, 0x15,        //   Report Count (21)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xF0,        //   Report ID (240)
	0x09, 0x47,        //   Usage (0x47)
	0x95, 0x3F,        //   Report Count (63)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xF1,        //   Report ID (241)
	0x09, 0x48,        //   Usage (0x48)
	0x95, 0x3F,        //   Report Count (63)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xF2,        //   Report ID (242)
	0x09, 0x49,        //   Usage (0x49)
	0x95, 0x0F,        //   Report Count (15)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xA7,        //   Report ID (167)
	0x09, 0x4A,        //   Usage (0x4A)
	0x95, 0x01,        //   Report Count (1)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xA8,        //   Report ID (168)
	0x09, 0x4B,        //   Usage (0x4B)
	0x95, 0x01,        //   Report Count (1)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xA9,        //   Report ID (169)
	0x09, 0x4C,        //   Usage (0x4C)
	0x95, 0x08,        //   Report Count (8)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xAA,        //   Report ID (170)
	0x09, 0x4E,        //   Usage (0x4E)
	0x95, 0x01,        //   Report Count (1)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xAB,        //   Report ID (171)
	0x09, 0x4F,        //   Usage (0x4F)
	0x95, 0x39,        //   Report Count (57)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xAC,        //   Report ID (172)
	0x09, 0x50,        //   Usage (0x50)
	0x95, 0x39,        //   Report Count (57)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xAD,        //   Report ID (173)
	0x09, 0x51,        //   Usage (0x51)
	0x95, 0x0B,        //   Report Count (11)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xAE,        //   Report ID (174)
	0x09, 0x52,        //   Usage (0x52)
	0x95, 0x01,        //   Report Count (1)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xAF,        //   Report ID (175)
	0x09, 0x53,        //   Usage (0x53)
	0x95, 0x02,        //   Report Count (2)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0x85, 0xB0,        //   Report ID (176)
	0x09, 0x54,        //   Usage (0x54)
	0x95, 0x3F,        //   Report Count (63)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)
	0xC0               // End Collection
};

HID_DESCRIPTOR G_Ds4HidDescriptor = {
	0x09,   // length of HID descriptor
	0x21,   // descriptor type == HID  0x21
	0x0100, // hid spec release
	0x00,   // country code == Not Specified
	0x01,   // number of HID class descriptors
	{                                       //DescriptorList[0]
		0x22,                               //report descriptor type 0x22
		sizeof(G_Ds4ReportDescriptor)       //total length of report descriptor
	}
};

// Xbox One compatible HID report descriptor (PID 0x02FF)
// This descriptor presents a standard report descriptor that xinputhid.sys will recognize.
HID_REPORT_DESCRIPTOR G_XboxReportDescriptor[] = {
	0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
	0x09, 0x05,        // Usage (Game Pad)
	0xA1, 0x01,        // Collection (Application)
	0x85, XB1_INPUT_REPORT_ID, //   Report ID (1)

	// Left thumbstick X, Y (16-bit signed)
	0x09, 0x30,        //   Usage (X)
	0x09, 0x31,        //   Usage (Y)
	0x16, 0x00, 0x80,  //   Logical Minimum (-32768)
	0x26, 0xFF, 0x7F,  //   Logical Maximum (32767)
	0x75, 0x10,        //   Report Size (16)
	0x95, 0x02,        //   Report Count (2)
	0x81, 0x02,        //   Input (Data,Var,Abs)

	// Right thumbstick Rx, Ry (16-bit signed)
	0x09, 0x33,        //   Usage (Rx)
	0x09, 0x34,        //   Usage (Ry)
	0x16, 0x00, 0x80,  //   Logical Minimum (-32768)
	0x26, 0xFF, 0x7F,  //   Logical Maximum (32767)
	0x75, 0x10,        //   Report Size (16)
	0x95, 0x02,        //   Report Count (2)
	0x81, 0x02,        //   Input (Data,Var,Abs)

	// Left trigger Z (10-bit unsigned)
	0x09, 0x32,        //   Usage (Z)
	0x15, 0x00,        //   Logical Minimum (0)
	0x26, 0xFF, 0x03,  //   Logical Maximum (1023)
	0x75, 0x0A,        //   Report Size (10)
	0x95, 0x01,        //   Report Count (1)
	0x81, 0x02,        //   Input (Data,Var,Abs)

	// Padding (6 bits)
	0x75, 0x06,        //   Report Size (6)
	0x95, 0x01,        //   Report Count (1)
	0x81, 0x03,        //   Input (Cnst,Var,Abs)

	// Right trigger Rz (10-bit unsigned)
	0x09, 0x35,        //   Usage (Rz)
	0x15, 0x00,        //   Logical Minimum (0)
	0x26, 0xFF, 0x03,  //   Logical Maximum (1023)
	0x75, 0x0A,        //   Report Size (10)
	0x95, 0x01,        //   Report Count (1)
	0x81, 0x02,        //   Input (Data,Var,Abs)

	// Padding (6 bits)
	0x75, 0x06,        //   Report Size (6)
	0x95, 0x01,        //   Report Count (1)
	0x81, 0x03,        //   Input (Cnst,Var,Abs)

	// D-Pad (Hat Switch)
	0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
	0x09, 0x39,        //   Usage (Hat switch)
	0x15, 0x00,        //   Logical Minimum (0)
	0x25, 0x07,        //   Logical Maximum (7)
	0x35, 0x00,        //   Physical Minimum (0)
	0x46, 0x3B, 0x01,  //   Physical Maximum (315)
	0x65, 0x14,        //   Unit (Eng Rot:Angular Pos)
	0x75, 0x08,        //   Report Size (8)
	0x95, 0x01,        //   Report Count (1)
	0x81, 0x42,        //   Input (Data,Var,Abs,Null)
	0x65, 0x00,        //   Unit (None)

	// 15 buttons: A, B, X, Y, LB, RB, Back, Start, Guide, LSB, RSB, Paddle1, Paddle2, Paddle3, Paddle4
	0x05, 0x09,        //   Usage Page (Button)
	0x19, 0x01,        //   Usage Minimum (1)
	0x29, 0x0F,        //   Usage Maximum (15)
	0x15, 0x00,        //   Logical Minimum (0)
	0x25, 0x01,        //   Logical Maximum (1)
	0x75, 0x01,        //   Report Size (1)
	0x95, 0x0F,        //   Report Count (15)
	0x81, 0x02,        //   Input (Data,Var,Abs)

	// Padding to 16-bit button boundary (1 bit)
	0x75, 0x01,        //   Report Size (1)
	0x95, 0x01,        //   Report Count (1)
	0x81, 0x03,        //   Input (Cnst,Var,Abs)

	// Vendor padding to fill 64-byte report
	0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
	0x09, 0x20,        //   Usage (0x20)
	0x75, 0x08,        //   Report Size (8)
	0x95, 0x2E,        //   Report Count (46)
	0x81, 0x02,        //   Input (Data,Var,Abs)

	// Output report for force feedback (HID PID Set Effect Report)
	0x05, 0x0F,        //   Usage Page (Physical Interface Device)
	0x09, 0x21,        //   Usage (Set Effect Report)
	0x85, XB1_OUTPUT_REPORT_ID,        //   Report ID (3)
	0xA1, 0x02,        //   Collection (Logical)
	0x09, 0x97,        //     Usage (DC Enable Actuators)
	0x15, 0x00,        //     Logical Minimum (0)
	0x25, 0x01,        //     Logical Maximum (1)
	0x75, 0x04,        //     Report Size (4)
	0x95, 0x01,        //     Report Count (1)
	0x91, 0x02,        //     Output (Data,Var,Abs)
	0x75, 0x04,        //     Report Size (4)
	0x95, 0x01,        //     Report Count (1)
	0x91, 0x03,        //     Output (Cnst,Var,Abs)
	0x09, 0x70,        //     Usage (Magnitude)
	0x15, 0x00,        //     Logical Minimum (0)
	0x25, 0x64,        //     Logical Maximum (100)
	0x75, 0x08,        //     Report Size (8)
	0x95, 0x04,        //     Report Count (4)
	0x91, 0x02,        //     Output (Data,Var,Abs)
	0x09, 0x50,        //     Usage (Duration)
	0x66, 0x01, 0x10,  //     Unit (Seconds)
	0x55, 0x0E,        //     Unit Exponent (-2)
	0x15, 0x00,        //     Logical Minimum (0)
	0x26, 0xFF, 0x00,  //     Logical Maximum (255)
	0x95, 0x01,        //     Report Count (1)
	0x91, 0x02,        //     Output (Data,Var,Abs)
	0x09, 0xA7,        //     Usage (Start Delay)
	0x91, 0x02,        //     Output (Data,Var,Abs)
	0x65, 0x00,        //     Unit (None)
	0x55, 0x00,        //     Unit Exponent (0)
	0x09, 0x7C,        //     Usage (Loop Count)
	0x91, 0x02,        //     Output (Data,Var,Abs)
	0xC0,              //   End Collection

	0xC0               // End Collection
};

HID_DESCRIPTOR G_XboxHidDescriptor = {
	0x09,   // length of HID descriptor
	0x21,   // descriptor type == HID  0x21
	0x0100, // hid spec release
	0x00,   // country code == Not Specified
	0x01,   // number of HID class descriptors
	{                                       //DescriptorList[0]
		0x22,                               //report descriptor type 0x22
		sizeof(G_XboxReportDescriptor)      //total length of report descriptor
	}
};

/// <summary>
/// Parses the PID from a device instance ID string.
/// Instance ID format: ROOT\VID_054C&PID_XXXX&DUOCONTROLLER&<token>\0000
/// </summary>
/// <param name="DeviceInstanceId">The device instance ID string.</param>
/// <returns>The parsed product ID, or 0 on failure.</returns>
USHORT GetPidFromInstanceId(_In_ PCWSTR DeviceInstanceId)
{
	PCWSTR pidStr = wcsstr(DeviceInstanceId, L"PID_");
	if (pidStr == NULL)
	{
		return 0;
	}

	pidStr += 4; // skip "PID_"

	// Parse up to 4 hex digits
	USHORT pid = 0;
	for (int i = 0; i < 4 && pidStr[i] != L'\0' && pidStr[i] != L'&'; i++)
	{
		pid <<= 4;
		WCHAR c = pidStr[i];
		if (c >= L'0' && c <= L'9')
		{
			pid += (USHORT)(c - L'0');
		}
		else if (c >= L'A' && c <= L'F')
		{
			pid += (USHORT)(c - L'A' + 10);
		}
		else if (c >= L'a' && c <= L'f')
		{
			pid += (USHORT)(c - L'a' + 10);
		}
		else
		{
			return 0;
		}
	}

	return pid;
}

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

		// Retrieve the device instance ID for unique pipe naming and PID detection
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

			// Determine controller sub type from PID in instance ID
			USHORT pid = GetPidFromInstanceId(deviceInstanceId);
			if (pid == 0x05C4)
			{
				deviceContext->ControllerSubType = DuoControllerSubTypeDualShock4;
			}
			else if (pid == 0x02FF)
			{
				deviceContext->ControllerSubType = DuoControllerSubTypeXbox;
			}
			else
			{
				deviceContext->ControllerSubType = DuoControllerSubTypeDualSense;
			}

			// Keep PID for product-string selection in Ioctl.c
			deviceContext->HidDeviceAttributes.ProductID = pid;

			// Generate a unique serial number from the device instance ID.
			// Instance ID format: ROOT\VID_054C&PID_XXXX&DUOCONTROLLER&<token>\0000
			// The <token> is unique per creation, so the serial stays unique across
			// pads even though the trailing instance number is reused by Windows.
			WCHAR* tokenStart = wcsrchr(deviceInstanceId, L'&');
			WCHAR* tokenEnd = wcsrchr(deviceInstanceId, L'\\');
			if (tokenStart != NULL && tokenEnd != NULL && tokenEnd > tokenStart)
			{
				size_t tokenLen = (size_t)(tokenEnd - tokenStart - 1);
				if (tokenLen < HID_DEVICE_SERIAL_NUMBER_MAX_LEN)
				{
					RtlCopyMemory(deviceContext->SerialNumber, tokenStart + 1, tokenLen * sizeof(WCHAR));
					deviceContext->SerialNumber[tokenLen] = L'\0';
				}
			}
			else
			{
				// Fallback: use the text after the last backslash.
				tokenStart = wcsrchr(deviceInstanceId, L'\\');
				if (tokenStart != NULL)
				{
					tokenStart++;
					wcscpy_s(deviceContext->SerialNumber, HID_DEVICE_SERIAL_NUMBER_MAX_LEN, tokenStart);
				}
				else
				{
					wcscpy_s(deviceContext->SerialNumber, HID_DEVICE_SERIAL_NUMBER_MAX_LEN, deviceInstanceId);
				}
			}

			WdfObjectDelete(deviceInstanceIdMemory);

			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Device instance ID: %ws", deviceContext->DeviceInstanceId);
		}

		// Configure device attributes, descriptor, and defaults based on controller sub type
		if (deviceContext->ControllerSubType == DuoControllerSubTypeXbox)
		{
			deviceContext->HidDeviceAttributes.VendorID = HID_DEVICE_VID_MICROSOFT;
			deviceContext->HidDeviceAttributes.VersionNumber = HID_DEVICE_VERSION;

			SetDefaultXboxReport(deviceContext->InputReport);

			deviceContext->HidDescriptor = G_XboxHidDescriptor;
			deviceContext->ReportDescriptor = G_XboxReportDescriptor;
		}
		else
		{
			deviceContext->HidDeviceAttributes.VendorID = HID_DEVICE_VID_SONY;
			deviceContext->HidDeviceAttributes.VersionNumber = HID_DEVICE_VERSION;

			if (deviceContext->ControllerSubType == DuoControllerSubTypeDualShock4)
			{
				SetDefaultDs4Report(deviceContext->InputReport);

				RtlZeroMemory(&deviceContext->DsOutputReport, sizeof(DS4_OUTPUT_REPORT));

				deviceContext->HidDescriptor = G_Ds4HidDescriptor;
				deviceContext->ReportDescriptor = G_Ds4ReportDescriptor;
			}
			else
			{
				SetDefaultDsReport(deviceContext->InputReport);

				RtlZeroMemory(&deviceContext->DsOutputReport, sizeof(DS_OUTPUT_REPORT));

				deviceContext->HidDescriptor = G_DsHidDescriptor;
				deviceContext->ReportDescriptor = G_DsReportDescriptor;
			}

			// If PID wasn't set from instance ID (e.g. zero), default to 0x0DF2
			if (deviceContext->HidDeviceAttributes.ProductID == 0)
			{
				deviceContext->HidDeviceAttributes.ProductID = 0x0DF2;
			}
		}

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
