#pragma once

#include <string>

namespace mdns
{
class MdnsResponder
{
public:
    // Uses the directory of the running executable.
    static int install();
    static int remove();

    // Optional: call mDNSResponder.exe from an explicit directory.
    static int installFromDir(const std::wstring& dir);
    static int removeFromDir(const std::wstring& dir);
};
} // namespace mdns
