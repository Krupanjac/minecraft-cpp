#include "Socket.h"  // Must be included first to handle Windows macros
#include "../Core/Logger.h"

namespace Network {

static bool g_networkInitialized = false;

bool initializeNetwork() {
    if (g_networkInitialized) return true;
    
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        LOG_ERROR("WSAStartup failed: " + std::to_string(result));
        return false;
    }
#endif
    
    g_networkInitialized = true;
    LOG_INFO("Network initialized");
    return true;
}

void shutdownNetwork() {
    if (!g_networkInitialized) return;
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    g_networkInitialized = false;
    LOG_INFO("Network shutdown");
}

Socket::Socket() : m_handle(INVALID_SOCKET_HANDLE) {}

Socket::Socket(SocketHandle handle) : m_handle(handle) {}

Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept 
    : m_handle(other.m_handle)
    , m_remoteAddress(std::move(other.m_remoteAddress))
    , m_remotePort(other.m_remotePort) {
    other.m_handle = INVALID_SOCKET_HANDLE;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        m_handle = other.m_handle;
        m_remoteAddress = std::move(other.m_remoteAddress);
        m_remotePort = other.m_remotePort;
        other.m_handle = INVALID_SOCKET_HANDLE;
    }
    return *this;
}

bool Socket::create() {
    if (m_handle != INVALID_SOCKET_HANDLE) {
        close();
    }
    
    m_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_handle == INVALID_SOCKET_HANDLE) {
        LOG_ERROR("Failed to create socket");
        return false;
    }
    
    return true;
}

void Socket::close() {
    if (m_handle != INVALID_SOCKET_HANDLE) {
#ifdef _WIN32
        closesocket(m_handle);
#else
        ::close(m_handle);
#endif
        m_handle = INVALID_SOCKET_HANDLE;
    }
}

bool Socket::isValid() const {
    return m_handle != INVALID_SOCKET_HANDLE;
}

bool Socket::bind(uint16_t port) {
    if (!isValid()) return false;
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (::bind(m_handle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        LOG_ERROR("Failed to bind socket to port " + std::to_string(port));
        return false;
    }
    
    return true;
}

bool Socket::listen(int backlog) {
    if (!isValid()) return false;
    
    if (::listen(m_handle, backlog) < 0) {
        LOG_ERROR("Failed to listen on socket");
        return false;
    }
    
    return true;
}

std::optional<Socket> Socket::accept() {
    if (!isValid()) return std::nullopt;
    
    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);
    
    SocketHandle clientHandle = ::accept(m_handle, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
    if (clientHandle == INVALID_SOCKET_HANDLE) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            LOG_ERROR("Accept failed: " + std::to_string(err));
        }
#else
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR("Accept failed: " + std::to_string(errno));
        }
#endif
        return std::nullopt;
    }
    
    Socket clientSocket(clientHandle);
    char addrStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, addrStr, INET_ADDRSTRLEN);
    clientSocket.m_remoteAddress = addrStr;
    clientSocket.m_remotePort = ntohs(clientAddr.sin_port);
    
    return clientSocket;
}

bool Socket::connect(const std::string& host, uint16_t port) {
    if (!isValid()) return false;
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    // Try to parse as IP address first
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        // Try DNS resolution
        struct addrinfo hints{}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        
        if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0 || !result) {
            LOG_ERROR("Failed to resolve host: " + host);
            return false;
        }
        
        addr.sin_addr = reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr;
        freeaddrinfo(result);
    }
    
    if (::connect(m_handle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS) {
            LOG_ERROR("Failed to connect to " + host + ":" + std::to_string(port) + " error: " + std::to_string(err));
            return false;
        }
#else
        if (errno != EINPROGRESS) {
            LOG_ERROR("Failed to connect to " + host + ":" + std::to_string(port));
            return false;
        }
#endif
    }
    
    m_remoteAddress = host;
    m_remotePort = port;
    return true;
}

int Socket::send(const void* data, size_t size) {
    if (!isValid()) return -1;
    
#ifdef _WIN32
    return ::send(m_handle, static_cast<const char*>(data), static_cast<int>(size), 0);
#else
    return ::send(m_handle, data, size, MSG_NOSIGNAL);
#endif
}

int Socket::receive(void* buffer, size_t size) {
    if (!isValid()) return -1;
    
#ifdef _WIN32
    return ::recv(m_handle, static_cast<char*>(buffer), static_cast<int>(size), 0);
#else
    return ::recv(m_handle, buffer, size, 0);
#endif
}

bool Socket::setNonBlocking(bool nonBlocking) {
    if (!isValid()) return false;
    
#ifdef _WIN32
    u_long mode = nonBlocking ? 1 : 0;
    return ioctlsocket(m_handle, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(m_handle, F_GETFL, 0);
    if (flags < 0) return false;
    
    if (nonBlocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    
    return fcntl(m_handle, F_SETFL, flags) == 0;
#endif
}

bool Socket::setReuseAddr(bool reuse) {
    if (!isValid()) return false;
    
    int value = reuse ? 1 : 0;
    return setsockopt(m_handle, SOL_SOCKET, SO_REUSEADDR, 
                     reinterpret_cast<const char*>(&value), sizeof(value)) == 0;
}

std::string Socket::getRemoteAddress() const {
    return m_remoteAddress;
}

uint16_t Socket::getRemotePort() const {
    return m_remotePort;
}

} // namespace Network
