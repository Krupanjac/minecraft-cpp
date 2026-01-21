#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <optional>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using SocketHandle = SOCKET;
    constexpr SocketHandle INVALID_SOCKET_HANDLE = INVALID_SOCKET;
    // Undef Windows macros that conflict with our code
    #ifdef ERROR
        #undef ERROR
    #endif
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <netdb.h>
    #include <errno.h>
    using SocketHandle = int;
    constexpr SocketHandle INVALID_SOCKET_HANDLE = -1;
#endif

namespace Network {

// Initialize/cleanup networking (call once at app start/end)
bool initializeNetwork();
void shutdownNetwork();

class Socket {
public:
    Socket();
    ~Socket();
    
    // Non-copyable, movable
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    
    // Create socket
    bool create();
    void close();
    bool isValid() const;
    
    // Server operations
    bool bind(uint16_t port);
    bool listen(int backlog = 10);
    std::optional<Socket> accept();
    
    // Client operations
    bool connect(const std::string& host, uint16_t port);
    
    // Data transfer
    int send(const void* data, size_t size);
    int receive(void* buffer, size_t size);
    
    // Utilities
    bool setNonBlocking(bool nonBlocking);
    bool setReuseAddr(bool reuse);
    
    std::string getRemoteAddress() const;
    uint16_t getRemotePort() const;
    
private:
    explicit Socket(SocketHandle handle);
    SocketHandle m_handle = INVALID_SOCKET_HANDLE;
    std::string m_remoteAddress;
    uint16_t m_remotePort = 0;
};

} // namespace Network
