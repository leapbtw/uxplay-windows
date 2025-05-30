# FREE AirPlay to your Windows PC
Free as both in "freedom" and "free beer"!

## Installation
1. Install the latest version of `Bonjour SDK for Windows` from the [Apple Developer website](https://developer.apple.com/download/all/?q=Bonjour%20SDK%20for%20Windows).
2. Download the latest version of uxplay-windows from [**releases**](https://github.com/leapbtw/uxplay-windows/releases/latest).

After installing, open the software. A new [tray icon](https://www.odu.edu/sites/default/files/documents/win10-system-tray.pdf) will appear near your clock—right-click it to start or stop AirPlay. You can also set it to run automatically when your PC starts

## FAQ
> What is uxplay-windows?

[UxPlay](https://github.com/FDH2/UxPlay/) allows you to screen share from your Apple devices to non-Apple ones using AirPlay.

`uxplay-windows` (this project) provides binaries of UxPlay for Windows 10/11 users, making it easier for those who may find compiling UxPlay challenging.
Most other software achieving the same functionality as `uxplay-windows` is usually paid and non-free.

> [!TIP]
> *I can't see my PC in AirPlay from my iPhone/iPad/Mac!!!*
> 1. Check if the server is running: click on the tray icon and restart it
> 2. Make sure you have [Bonjour](https://developer.apple.com/download/all/?q=Bonjour%20SDK%20for%20Windows) installed on your PC
> 3. Toggle Airplane Mode on your iPhone/iPad/Mac twice and try again

> [!IMPORTANT]
> *Why is Windows Defender complaining during installation?*
> 
> ![alt text](https://raw.githubusercontent.com/leapbtw/uxplay-windows/refs/heads/main/stuff/defender.png "defender")
>
> Just click on `More info` and it will let you install. It complains because the executable is not signed. If you don't trust this software you can always build it yourself! See below.  

> How do I build this software myself?

Please see [BUILDING.md](./BUILDING.md)


## TODO
- make colored icon to show if uxplay is running or not
- make an update checker
- modify github action
  - stop using sdk and use PS instead
  - do `setup.exe /extract` to get the msi (instead of using 7zip)


## Reporting Issues
Please report issues related to the build system created with GitHub Actions in this repository. For issues related to other parts of this software, report them in their respective repositories.

## License
I haven't made most of the code myself, please look at [CREDITS](./CREDITS.md). If you want to use this project for anything, please take a look at the [LICENSE](./LICENSE).
