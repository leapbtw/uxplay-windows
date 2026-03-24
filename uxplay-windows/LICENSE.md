# Project License Information

This software (uxplay-windows) is licensed under the [GNU General Public License v3.0 (GPLv3)](https://www.gnu.org/licenses/gpl-3.0.html).

uxplay-windows makes use of various software components and libraries, each under their own licenses.

### 1. UxPlay
- License: [GNU General Public License v3.0 (GPLv3)](https://github.com/FDH2/UxPlay/blob/master/LICENSE)
- Source: https://github.com/FDH2/UxPlay

### 2. mDNSResponder
- uxplay-windows is compiled with both parts of mDNSShared and mDNSCore
- Licenses: [Three-Clause BSD License](https://opensource.org/licenses/BSD-3-Clause) / [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0)
- Sources:  
  - https://github.com/apple-oss-distributions/mDNSResponder/blob/rel/mDNSResponder-214/LICENSE  
  - https://github.com/apple-oss-distributions/mDNSResponder/blob/rel/mDNSResponder-214/mDNSShared/dns_sd.h  
  - https://github.com/apple-oss-distributions/mDNSResponder/blob/rel/mDNSResponder-214/mDNSCore/mDNS.c

### 3. GStreamer
- uxplay-windows redistributes parts of GStreamer and its plugins.
- License: [Lesser General Public License (LGPL)](https://www.gnu.org/licenses/lgpl-3.0.html)
- Users must comply with LGPL terms when redistributing the software.
- Sources:  
  - https://gstreamer.freedesktop.org/  
  - https://github.com/GStreamer/gst-plugins-base  
  - https://github.com/GStreamer/gst-plugins-bad  
  - https://github.com/GStreamer/gst-plugins-good  
  - https://github.com/GStreamer/gst-libav

### 4. Inno Setup
- License: [Inno Setup License](https://jrsoftware.org/isinfo.php#license)
- Source: https://jrsoftware.org/isinfo.php

### 5. Material Design Icon File
- uxplay-windows uses a slightly modified icon from Material Design Icons
- License: [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0)
- Source: https://pictogrammers.com/docs/general/license/

---

### Build Tools

uxplay-windows uses **MSYS2** and **PyInstaller** during the build process but **does not redistribute these tools**.

- PyInstaller is licensed under GPL 2.0 with a special exception allowing bundling of commercial applications without imposing GPL on the final app. Since uxplay-windows does not modify or redistribute PyInstaller, no additional licensing obligations arise.

- MSYS2 is a collection of software packages, each with its own license. Since uxplay-windows does not redistribute MSYS2 components, you only need to comply with the licenses of any third-party libraries (Gstreamer) included in uxplay-windows.

---

## Disclaimer

The license information provided here is based on my best understanding of the licenses of the respective components at the time of this writing.  
I make no guarantee that it is accurate or complete. Users of this project are responsible for ensuring compliance with the licenses of all included components.

If there's any problem or question regarding licensing, please contact me and I will respond as soon as possible to address your concerns.
