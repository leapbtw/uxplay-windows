# uxplay-windows: AirPlay to your Windows PC !

[UxPlay](https://github.com/FDH2/UxPlay/) allows you to screen share from your Apple devices to non-Apple ones using AirPlay.

**uxplay-windows** (this project) provides binary builds of UxPlay for Windows 10/11 users, making it easier for those who may find compiling UxPlay challenging. Most other software achieving the same functionality as UxPlay is usually paid and non-free. 

You can download uxplay-windows from here: [**DOWNLOAD**](https://github.com/leapbtw/uxplay-windows/releases/latest)

## Requirements
- Windows 10 or 11
- [Bonjour SDK for Windows v3.0](https://developer.apple.com/bonjour/) installed on your PC.

## Installation
1. Download the latest version of uxplay-windows from the [**releases page**](https://github.com/leapbtw/uxplay-windows/releases/latest).
2. Install the latest version of Bonjour SDK for Windows from the [Apple Developer website](https://developer.apple.com/bonjour/).

## Building from Source
This project also provides GitHub Actions to let you compile the software yourself easily. Follow these steps:

1. Fork the repo.
2. Download the [Bonjour SDK for Windows](https://developer.apple.com/bonjour/).
3. Upload the `bonjoursdksetup.exe` to a cloud storage service and get the direct URL.
4. Provide the direct URL to the GitHub Action, which will handle the rest.

The resulting uxplay-windows installer will be provided as an artifact from the GitHub Action. After you're done, please remove the Bonjour SDK from any cloud storage platform, as Apple provides it for personal use and it should not be redistributed.

## Credits
This project uses code and resources from:
- [UxPlay](https://github.com/FDH2/UxPlay/)
- [MSYS2](https://www.msys2.org/)
- [7-zip](https://7-zip.org/)
- Icon from [Material Design Icons](https://pictogrammers.com/library/mdi/)
- [Inno Setup](https://jrsoftware.org/isinfo.php)
- [Bonjour SDK for Windows](https://developer.apple.com/bonjour/)

## Reporting Issues
Please report issues related to the build system created with GitHub Actions in this repository. For issues related to other parts of this software, report them in their respective repositories.

## License
If you want to use this project for anything, please take a look at the [LICENSE](./LICENSE).

