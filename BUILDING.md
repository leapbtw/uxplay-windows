## Building from Source
This project also provides GitHub Actions to let you compile the software yourself easily. Follow these steps:

1. Fork the repo.
2. Download and install the latest version of [Bonjour SDK for Windows](https://developer.apple.com/bonjour/).
3. Upload the `bonjoursdksetup.exe` to a cloud storage service and get the **direct URL** for the file.
4. Provide the direct URL to the GitHub Action, which will handle the rest.

The resulting uxplay-windows installer will be provided as an artifact from the GitHub Action. After you're done, please remove the Bonjour SDK from any cloud storage platform, as Apple provides it for personal use and it should not be redistributed.
