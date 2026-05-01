#include "mdns_responder.hpp"

#include <windows.h>
#include <shellapi.h>

#include <iostream>
#include <stdexcept>

namespace mdns
{
static std::wstring getProcessExeDir()
{
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        throw std::runtime_error("GetModuleFileNameW failed");
    }

    std::wstring fullPath(buf, len);
    size_t pos = fullPath.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        throw std::runtime_error("Unable to determine module directory");
    }
    return fullPath.substr(0, pos);
}

static int launchMdnsResponder(const std::wstring& dir,
                                const std::wstring& params)
{
    std::wstring exePath = dir + L"\\mDNSResponder.exe";

    STARTUPINFOW si{};
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi{};

    std::wstring cmdLine = L"\"" + exePath + L"\" " + params;

    BOOL ok = CreateProcessW(
        nullptr,
        cmdLine.data(),
        nullptr, nullptr,
        FALSE,
        0,
        nullptr,
        dir.c_str(),
        &si,
        &pi
    );

    if (ok) {
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        return exitCode == 0 ? 0 : 2;
    }

    DWORD err = GetLastError();
    if (err != 740) { // 740 = ERROR_ELEVATION_REQUIRED
        std::wcerr << L"CreateProcessW failed. Error=" << err << L"\n";
        return 1;
    }

    // Retry with UAC elevation
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = exePath.c_str();
    sei.lpParameters = params.c_str();
    sei.lpDirectory = dir.c_str();
    sei.nShow = SW_SHOWDEFAULT;

    if (!ShellExecuteExW(&sei)) {
        DWORD err2 = GetLastError();
        std::wcerr << L"ShellExecuteExW(runas) failed. Error=" << err2
                   << L"\n";
        return 1;
    }

    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(sei.hProcess, &exitCode);

        CloseHandle(sei.hProcess);

        return exitCode == 0 ? 0 : 2;
    }

    return 0;
}

int MdnsResponder::installFromDir(const std::wstring& dir)
{
    return launchMdnsResponder(dir, L"-install");
}

int MdnsResponder::removeFromDir(const std::wstring& dir)
{
    return launchMdnsResponder(dir, L"-remove");
}

int MdnsResponder::install()
{
    return installFromDir(getProcessExeDir());
}

int MdnsResponder::remove()
{
    return removeFromDir(getProcessExeDir());
}
} // namespace mdns
