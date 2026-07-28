## Building from source

The ARM64 build is fully automated by GitHub Actions:

1. [Fork the repository](https://github.com/leapbtw/uxplay-windows/fork).
2. Open the **Actions** tab.
3. Select **build uxplay-windows ARM64** and run it.
4. Download the `uxplay-windows-arm64` artifact.

The artifact contains both the MSI installer and a portable ZIP. The workflow
builds all dependencies, discovers the current Qt and GStreamer runtime DLLs,
and verifies the bundle without using libraries installed on the runner.

For local development, see [DEVELOPERS-GUIDE.md](./DEVELOPERS-GUIDE.md).
