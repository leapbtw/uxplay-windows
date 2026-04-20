This is a quick developer guide for people who want to locally build this branch of uxplay-windows and hopefully contribute to the project

- clone the github repo
  ```git
  git clone https://github.com/leapbtw/uxplay-windows.git
  ```
  
- install [MSYS2](https://www.msys2.org/)
  <h6>from now on we'll be working in UCRT64, so my suggestion is to add it to Windows Terminal to make it easier to work with</h6>
- install the UCRT64 dependencies with pacman
  ```
  pacman -S --needed - < ucrt_deps.txt
  ```
- install the BLE beacon dependencies with pip
  ```python
  pip install winrt-Windows.Foundation winrt-Windows.Foundation.Collections winrt-Windows.Devices.Bluetooth.Advertisement winrt-Windows.Storage.Streams --break-system-packages
  ```
  <h6>--break-system-packages must be used as per original UxPlay instructions in their README</h6>
- finally we can build uxplay-windows, simply run `./make_release.sh`. \
  Once it's built, we need to package all the gstreamer DLL(s) but they are loaded dynamically, so uxplay-windows is automatically ran, just connect your device and press ENTER. \
  The artifact is in the `release/` folder
