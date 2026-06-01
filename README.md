<p align="center">
  <img src=".github/images/logo.png" alt="Logo" width="256" />
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Windows-10+-0078D6?style=for-the-badge&logo=windows&logoColor=white" />
  <img src="https://img.shields.io/badge/UMDF-2.0-7F52FF?style=for-the-badge&logo=windows&logoColor=white" />
  <img src="https://img.shields.io/badge/x64-00599C?style=for-the-badge&logo=c&logoColor=white" />
  <img src="https://img.shields.io/badge/License-Apache%202.0-yellow?style=for-the-badge" />
</p>

## Overview

**DuoController** is a pure userspace virtual gamepad library and UMDF driver for Windows that emulates **DualSense Edge** and **Xbox One** controllers entirely from user mode.

It is a spiritual successor to [ViGEmBus](https://github.com/ViGEm/ViGEmBus), the now-abandoned KMDF-based kernel driver, reimagined using Microsoft's **User-Mode Driver Framework (UMDF)**.

## Why Userspace?

Microsoft is actively steering the Windows driver ecosystem away from kernel-mode and toward user-mode drivers.

The reasoning is clear:

| Concern | Kernel Driver (KMDF) | User Driver (UMDF) |
|---|---|---|
| **System Stability** | A crash takes down the entire OS | A crash takes down only the driver process |
| **Attestation Signing** | Required (EV certificate, expensive) | **Not required** |
| **Development Complexity** | High - kernel debugging, WinDbg, VM setup | Low - standard user-mode tooling |
| **Security Surface** | Ring 0 - full system access | Ring 3 - sandboxed, limited privileges |
| **Distribution** | Signing portal, HLK testing | Standard DLL deployment |

### The Signing Problem

Kernel drivers on modern Windows **must** be submitted to the Windows Hardware Dev Center, pass through attestation signing, and have to be pre-signed by an Extended Validation (EV) code signing certificate.

This process costs hundreds of dollars, requires a registered legal entity that fulfills Microsoft's requirements, and takes weeks of turnaround.

UMDF drivers require **no such signing**.

A standard code signing certificate (or even self-signing for development or small-scale publishing) is sufficient.

This makes DuoController dramatically more accessible to:

- **Independent developers**
- **Small teams**

## Features

- **Full DualSense Edge Controller Emulation** - Via HID minidriver
- **Full Xbox One Controller Emulation** - Via `xboxgipsynthetic.dll`
- **Session Isolation** - Isolates gamepads to the current session ID for multi-session and RDP environments
- **Minimal Footprint** - A single DLL/INF pair, no kernel components

## Architecture

```mermaid
flowchart TB
    App["<b>Your Service</b><br/><span style='white-space:nowrap;color:inherit;font-size:small'>Running as SYSTEM</span><br/><br/>DuoController_Initialize()<br/><span style='white-space:nowrap;color:inherit'>DuoController_CreateController(Ds|Xbox)</span><br/>DuoController_SendReport()<br/>DuoController_SendReportDs()<br/>DuoController_RemoveController()<br/>DuoController_Uninitialize()"]
    DS["<b>DualSense Path</b><br/><br/>Shared Memory<br/>(UMDF Driver)"]
    Xbox["<b>Xbox Path</b><br/><br/>xboxgipsynthetic.dll<br/>(System API)"]
    Input["<b>Windows Game Input API</b><br/><span style='white-space:nowrap;color:inherit'>(GameInput / XInput / DirectInput / RawInput)</span>"]

    App --> DS
    App --> Xbox
    DS --> Input
    Xbox --> Input

    style App fill:#1a1a2e,stroke:#e94560,stroke-width:2px,color:#fff
    style DS fill:#16213e,stroke:#0f3460,stroke-width:2px,color:#fff
    style Xbox fill:#16213e,stroke:#0f3460,stroke-width:2px,color:#fff
    style Input fill:#1a1a2e,stroke:#e94560,stroke-width:2px,color:#fff
```

## API

```c
// Initialize the library
HRESULT DuoController_Initialize();

// Create a virtual controller
HRESULT DuoController_CreateController(
    DUO_CONTROLLER_TYPE controllerType,    // DuoControllerTypeXbox or DuoControllerTypeDs
    DuoController_VibrationReportCallback_t vibrationCallback,
    void* vibrationCallbackContext,
    void** controller
);

// Send Xbox One input report
HRESULT DuoController_SendReport(
    void* controller,
    DUO_CONTROLLER_INPUT_REPORT* inputReport
);

// Send DualSense Edge input report
HRESULT DuoController_SendReportDs(
    void* controller,
    DUO_CONTROLLER_INPUT_REPORT_DS* inputReport
);

// Remove a virtual controller
HRESULT DuoController_RemoveController(void* controller);

// Uninitialize the library
HRESULT DuoController_Uninitialize();
```

## Getting Started

### Prerequisites

- Windows 10 or later (x64)
- Visual Studio 2026
- Windows Driver Kit (WDK, Nu-Get preferred)

### Build

```powershell
# Open DuoController.sln in Visual Studio and build for Release x64
```

## Projects

| Project | Description |
|---|---|
| `DuoController` | The UMDF driver DLL and library — implements the HID minidriver, shared memory server, and the client API |
| `DuoControllerSample` | A minimal C sample showing library usage for both Xbox One and DualSense Edge controller types |

## License

Apache 2.0 © 2026 Black-Seraph
