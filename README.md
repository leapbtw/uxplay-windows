# uxplay-windows: UxPlay AirPlay-Mirror and AirPlay-Audio for Windows

[UxPlay](https://github.com/FDH2/UxPlay/) lets you screenshare from your Apple devices, to non-apple ones with AirPlay.

uxplay-windows (this project) provides binary builds of UxPlay for Windows 10/11 users, because compiling UxPlay might be difficult for some, and other softwares that achieve the same thing as UxPlay are usually paid and non-free.

You can download uxplay-windows from here: [**DOWNLOAD**](https://github.com/leapbtw/uxplay-windows/releases/latest)

This project is also provides Github Actions to let you easily compile the software yourself without too much hassle. Just fork the repo, download [Bonjour SDK for Windows](https://developer.apple.com/bonjour/), re-upload it somewhere, get the direct URL to the **bonjoursdksetup.exe** and provide it to the Github Action, which will do the rest for you. The resulting uxplay-windows installer will be provided as artifact from the Github Action. After you're done, please take down Bonjour SDK from any cloud storage platform, since Apple provides it for personal use and should not be redistributed.

This project uses the code from [UxPlay](https://github.com/FDH2/UxPlay/), [MSYS2](https://www.msys2.org/), [7-zip](https://7-zip.org/), an icon from [Material Design Icons](https://pictogrammers.com/library/mdi/) and [Inno Setup](https://jrsoftware.org/isinfo.php) to create the installer. It also uses [Bonjour SDK for Windows](https://developer.apple.com/bonjour/) which should be provided by the user who's supposed to have downloaded it from Apple directly after logging in with their Apple Developer account.

In this repo you should only report issues related to the build system I made with Github Actions, and in the other respective placesissuesissues related to other parts of this software

If you want to use this project for anything please take a look at [LICENSE](./LICENSE)
