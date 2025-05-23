#include <winsock2.h>
#include <Ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

namespace {

class WinSock {
private:
  WinSock() noexcept {}

public:
  static WinSock* create() {
    WSADATA wsa;
    return SUCCEEDED(WSAStartup(MAKEWORD(2, 2), &wsa)) ? new WinSock() : nullptr;
  }

  WinSock(const WinSock&) noexcept = delete;

  WinSock(WinSock&&) noexcept = delete;

  ~WinSock() noexcept {
    WSACleanup();
  }

  class Socket {
  private:
    friend class WinSock;

    SOCKET sock_;
    sockaddr_in addr_;

    Socket(SOCKET sock, sockaddr_in addr) noexcept : sock_(sock), addr_(addr) {}

  public:
    Socket(const Socket&) noexcept = delete;

    Socket(Socket&&) noexcept = delete;

    int send(const char* buffer, int len, int flags = 0) noexcept {
      return sendto(sock_, buffer, len, flags, reinterpret_cast<sockaddr*>(&addr_), sizeof(addr_));
    }

    int receive(char* buffer, int len, int flags = 0) noexcept {
      return recv(sock_, buffer, len, flags);
    }

    ~Socket() noexcept {
      closesocket(sock_);
    }
  };

  Socket* createListenerUDP(int port) {
    if (SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); sock != INVALID_SOCKET) {
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);
      addr.sin_addr.s_addr = INADDR_ANY;
      if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
        return new Socket(sock, addr);
      }
      closesocket(sock);
    }
    return nullptr;
  }

  Socket* createSenderUDP(const TCHAR* ip, int port) {
    if (SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); sock != INVALID_SOCKET) {
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);
      if (InetPton(AF_INET, ip, &addr.sin_addr) == 1) {
        return new Socket(sock, addr);
      }
    }
    return nullptr;
  }
};

} // namespace
