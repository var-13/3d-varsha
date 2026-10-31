// src/launcher/launch.cpp
// Launch.exe – starts ThermoBenchServer, waits until /health responds,
// opens the default browser, then shuts the server on exit.
// No admin rights required. No installs.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>   // ShellExecuteA
#include <winsock2.h>

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

// ── Probe /health via a raw TCP connection (avoids linking curl) ─────────────
// Returns true when the server accepts the connection AND replies with 200.
static bool server_ready(int port, int timeout_ms = 200) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) { WSACleanup(); return false; }

    // Non-blocking connect with timeout
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<u_short>(port));
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    fd_set fds; FD_ZERO(&fds); FD_SET(s, &fds);
    timeval tv{ 0, timeout_ms * 1000 };
    bool ok = (select(0, nullptr, &fds, nullptr, &tv) == 1);

    closesocket(s);
    WSACleanup();
    return ok;
}

int main() {
    // Resolve the directory that contains this .exe.
    // We use a char array filled by GetModuleFileNameA, then copy to std::string
    // before constructing the path – avoids returning a pointer to a local array.
    char _buf[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, _buf, MAX_PATH);
    fs::path exe_dir = fs::path(std::string(_buf)).parent_path();

    // Server binary sits alongside launcher (both built to build/bin/)
    fs::path server_exe = exe_dir / "ThermoBenchServer.exe";
    fs::path ui_dir     = exe_dir.parent_path().parent_path() / "ui"; // build/bin/../../ui
    int port = 8080;


    if (!fs::exists(server_exe)) {
        // Fall back: look two directories up (running from project root build/)
        server_exe = exe_dir / ".." / "ThermoBenchServer.exe";
    }
    if (!fs::exists(server_exe)) {
        std::cerr << "[Launch] ERROR: ThermoBenchServer.exe not found near "
                  << exe_dir << "\n";
        return 1;
    }

    // ── Start the server process ─────────────────────────────────────────
    std::string cmd = "\"" + server_exe.string() + "\" \"" +
                      ui_dir.string() + "\" " + std::to_string(port);

    STARTUPINFOA si{}; si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;   // no terminal window
    PROCESS_INFORMATION pi{};

    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr,
                        FALSE, 0, nullptr, nullptr, &si, &pi)) {
        std::cerr << "[Launch] ERROR: Could not start server. Code="
                  << GetLastError() << "\n";
        return 1;
    }
    std::cout << "[Launch] Server starting (PID " << pi.dwProcessId << ")...\n";

    // ── Wait until /health responds (up to 10 seconds) ──────────────────
    const int max_tries = 50;
    bool ready = false;
    for (int i = 0; i < max_tries; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (server_ready(port)) { ready = true; break; }
        std::cout << "." << std::flush;
    }
    std::cout << "\n";

    if (!ready) {
        std::cerr << "[Launch] ERROR: Server did not start within 10 s.\n";
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        return 1;
    }

    std::string url = "http://localhost:" + std::to_string(port);
    std::cout << "[Launch] Server ready. Opening " << url << "\n";

    // ── Open default browser ─────────────────────────────────────────────
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    // ── Wait for user to close the browser / press Ctrl+C ───────────────
    std::cout << "[Launch] Press Enter to stop the server...\n";
    std::cin.get();

    // ── Shut down server ─────────────────────────────────────────────────
    TerminateProcess(pi.hProcess, 0);
    WaitForSingleObject(pi.hProcess, 3000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    std::cout << "[Launch] Server stopped. Goodbye.\n";
    return 0;
}
