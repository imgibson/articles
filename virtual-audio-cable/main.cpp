#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>

#include <iostream>
#include <thread>

#include "audio.inl"
#include "winsock.inl"

#define VERSION 1
#define FRAMES_PER_BUFFER 512

namespace std {
#ifdef UNICODE
  wostream& tcout = wcout;
  wistream& tcin = wcin;
  wostream& tcerr = wcerr;
#else
  ostream& tcout = cout;
  istream& tcin = cin;
  ostream& tcerr = cerr;
#endif // UNICODE
} // namespace std

namespace {

template <typename T>
class Scope {
private:
  T* ptr_;

public:
  explicit Scope(T* ptr = nullptr) noexcept
    : ptr_{ptr} {}

  Scope(const Scope&) noexcept = delete;

  Scope(Scope&&) noexcept = delete;

  ~Scope() noexcept {
    delete ptr_;
  }

  operator T*() const noexcept {
    return ptr_;
  }

  Scope& operator=(T* ptr) noexcept {
    delete ptr_;
    ptr_ = ptr;
    return *this;
  }

  T* operator->() const noexcept {
    return ptr_;
  }

  T& operator*() const noexcept {
    return *ptr_;
  }
};

struct PacketHeader {
  UINT16 version;
  UINT16 seq_num;
};

bool recv_task(INT sampleRate, INT channels, INT bitDepth, INT port) {
  Scope<Audio> audio(Audio::create());  
  if (!audio) {
    std::tcerr << _T("Failed to initialize audio library") << std::endl;
    return false;
  }

  Scope<WinSock> winsock(WinSock::create());
  if (!winsock) {
    std::tcerr << _T("Failed to initialize WinSock") << std::endl;
    return false;
  }

  Scope<WinSock::Socket> socket(winsock->createListenerUDP(port));
  if (!socket) {
    std::tcerr << _T("Failed to create socket") << std::endl;
    return false;
  }

  Scope<Audio::Device::Names<1024>> names(Audio::Device::Names<1024>::create());
  if (!names) {
    std::tcerr << _T("Failed to create device names") << std::endl;
    return false;
  }

  if (!audio->initRenderNames(*names)) {
    std::tcerr << _T("Failed to enumerate devices") << std::endl;
    return false;
  }

  UINT index = 0;
  std::tcout << _T("Available output audio devices:") << std::endl;
  names->forEach([&index](const TCHAR* name) { std::tcout << index++ << ": " << name << std::endl; });

  std::tcout << std::endl << _T("Enter the device index to use: ");
  std::tcin >> index;

  Scope<Audio::Render> rd(audio->createRenderDevice(index, channels, sampleRate, bitDepth));
  if (!rd) {
    std::tcerr << _T("Failed to initialize audio device") << std::endl;
    return false;
  }

  UINT32 bufferFrameCount;
  rd->client()->GetBufferSize(&bufferFrameCount);
  rd->client()->Start();
  
  const UINT bufferSize = FRAMES_PER_BUFFER * (rd->getFormat().nChannels * rd->getFormat().wBitsPerSample) / 8;
  const UINT headerSize = sizeof(PacketHeader);
  const UINT packetSize = headerSize + bufferSize;
  char* buffer = static_cast<char*>(malloc(packetSize));
  if (!buffer) {
    std::tcerr << _T("Failed to allocate buffer") << std::endl;
    return false;
  }

  UINT16 next_seq = 0;
  std::tcout << _T("Receiving audio... press Ctrl+C to stop") << std::endl;

  while (true) {
    WaitForSingleObject(rd->getEvent(), INFINITE);

    UINT32 padding;
    rd->client()->GetCurrentPadding(&padding);

    UINT32 framesAvailable = bufferFrameCount - padding;
    if (framesAvailable < FRAMES_PER_BUFFER) {
      continue;
    }

    BYTE* data;
    rd->renderClient()->GetBuffer(FRAMES_PER_BUFFER, &data);
    int received = socket->receive(buffer, packetSize);
    if (received == SOCKET_ERROR) {
      std::tcerr << _T("Socket error: ") << WSAGetLastError() << std::endl;
      break;
    }

    PacketHeader* packetHeader = reinterpret_cast<PacketHeader*>(buffer);
    if (packetHeader->version != VERSION) {
      std::tcerr << _T("Version mismatch") << std::endl;
      continue;
    }

    if (next_seq++ != packetHeader->seq_num) {
      int delta = packetHeader->seq_num - next_seq + 1;
      std::tcerr << _T("Dropped ") << delta << _T(" packets") << std::endl;
      next_seq = packetHeader->seq_num + 1;
    }

    CopyMemory(data, &buffer[headerSize], bufferSize);
    rd->renderClient()->ReleaseBuffer(FRAMES_PER_BUFFER, 0);
  }

  rd->client()->Stop();
  return true;
}

bool send_task(INT sampleRate, INT channels, INT bitDepth, const TCHAR* ip, INT port) {
  Scope<Audio> audio(Audio::create());
  if (!audio) {
    std::tcerr << _T("Failed to initialize audio library") << std::endl;
    return false;
  }

  Scope<WinSock> ws(WinSock::create());
  if (!ws) {
    std::tcerr << _T("Failed to initialize WinSock") << std::endl;
    return false;
  }

  Scope<WinSock::Socket> socket(ws->createSenderUDP(ip, port));
  if (!socket) {
    std::tcerr << _T("Failed to create socket") << std::endl;
    return false;
  }

  Scope<Audio::Device::Names<1024>> names(Audio::Device::Names<1024>::create());
  if (!names) {
    std::tcerr << _T("Failed to create device names") << std::endl;
    return false;
  }

  if (!audio->initCaptureNames(*names)) {
    std::tcerr << _T("Failed to enumerate devices") << std::endl;
    return false;
  }

  UINT index = 0;
  std::tcout << _T("Available input audio devices:") << std::endl;
  names->forEach([&index](const TCHAR* name) noexcept { std::tcout << index++ << ": " << name << std::endl; });

  std::tcout << std::endl << _T("Enter the device index to use: ");
  std::tcin >> index;

  Scope<Audio::Capture> rd(audio->createCaptureDevice(index, channels, sampleRate, bitDepth));
  if (!rd) {
    std::tcerr << _T("Failed to initialize audio device") << std::endl;
    return false;
  }

  rd->client()->Start();

  std::tcout << _T("Sending audio... press Enter to stop") << std::endl;
  bool running = true;
  std::thread stopper([&]() {
    std::tcin.get();
    //running = false;
  });

  const UINT bufferSize = FRAMES_PER_BUFFER * (rd->getFormat().nChannels * rd->getFormat().wBitsPerSample) / 8;
  const UINT headerSize = sizeof(PacketHeader);
  const UINT packetSize = headerSize + bufferSize;
  char* buffer = static_cast<char*>(malloc(packetSize));
  if (!buffer) {
    std::tcerr << _T("Failed to allocate buffer") << std::endl;
    return false;
  }

  UINT bytesDone = 0;
  UINT16 nextSeq = 0;

  while (running) {
    WaitForSingleObject(rd->getEvent(), INFINITE);

    UINT32 captureSize = 0;
    rd->captureClient()->GetNextPacketSize(&captureSize);
    while (captureSize > 0) {
      BYTE* data;
      DWORD flags;
      UINT32 numFrames;
      if (FAILED(rd->captureClient()->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr))) {
        break;
      }

      UINT bytesSent = 0;
      UINT bytesLeft = numFrames * rd->getFormat().nBlockAlign;
      while (bytesLeft > 0) {
        if (bytesDone + bytesLeft < bufferSize) {
          CopyMemory(&buffer[headerSize + bytesDone], &data[bytesSent], bytesLeft);
          bytesDone += bytesLeft;
          bytesSent += bytesLeft;
          bytesLeft = 0;
        } else {
          UINT bytesAvail = bufferSize - bytesDone;
          CopyMemory(&buffer[headerSize + bytesDone], &data[bytesSent], bytesAvail);
          bytesDone = 0;
          bytesSent += bytesAvail;
          bytesLeft -= bytesAvail;

          PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
          header->version = VERSION;
          header->seq_num = nextSeq++;
          socket->send(buffer, packetSize);
        }
      }

      rd->captureClient()->ReleaseBuffer(numFrames);
      rd->captureClient()->GetNextPacketSize(&captureSize);
    }
  }

  rd->client()->Stop();
  return true;
}

} // namespace

int _tmain(int argc, TCHAR* argv[]) {
  INT port = 5004;
  INT channels = 2;
  INT bitDepth = 32;
  INT sampleRate = 44100;  
  const TCHAR* destIp = nullptr;

  for (int i = 1; i < argc; ++i) {
    if (_tcscmp(argv[i], _T("--help")) == 0) {
      std::tcout << _T("Usage: ") << argv[0] << _T(" [options]") << std::endl;
      std::tcout << _T("Options:") << std::endl;
      std::tcout << _T("  --help                     Show this help message") << std::endl;
      std::tcout << _T("  --dest, -d <ip>            Destination IP address") << std::endl;
      std::tcout << _T("  --port, -p <port>          Listen/send port number (default: 5004)") << std::endl;
      std::tcout << _T("  --channels, -c <channels>  Number of audio channels (default: 2)") << std::endl;
      std::tcout << _T("  --bitdepth, -b <depth>     Bit depth (default: 16)") << std::endl;
      std::tcout << _T("  --samplerate, -s <rate>    Sample rate (default: 48000)") << std::endl;
      return EXIT_SUCCESS;
    }
    if (_tcscmp(argv[i], _T("--dest")) == 0 || _tcscmp(argv[i], _T("-d")) == 0) {
      if (i + 1 >= argc) {
        std::tcerr << _T("Missing argument for '") << argv[i] << _T("'") << std::endl;
        return EXIT_FAILURE;
      }
      destIp = argv[++i];
    } else if (_tcscmp(argv[i], _T("--port")) == 0 || _tcscmp(argv[i], _T("-p")) == 0) {
      if (i + 1 >= argc) {
        std::tcerr << _T("Missing argument for '") << argv[i] << _T("'") << std::endl;
        return EXIT_FAILURE;
      }
      port = _tstoi(argv[++i]);
    } else if (_tcscmp(argv[i], _T("--channels")) == 0 || _tcscmp(argv[i], _T("-c")) == 0) {
      if (i + 1 >= argc) {
        std::tcerr << _T("Missing argument for '") << argv[i] << _T("'") << std::endl;
        return EXIT_FAILURE;
      }
      channels = _tstoi(argv[++i]);
    } else if (_tcscmp(argv[i], _T("--bitdepth")) == 0 || _tcscmp(argv[i], _T("-b")) == 0) {
      if (i + 1 >= argc) {
        std::tcerr << _T("Missing argument for '") << argv[i] << _T("'") << std::endl;
        return EXIT_FAILURE;
      }
      bitDepth = _tstoi(argv[++i]);
    } else if (_tcscmp(argv[i], _T("--samplerate")) == 0 || _tcscmp(argv[i], _T("-s")) == 0) {
      if (i + 1 >= argc) {
        std::tcerr << _T("Missing argument for '") << argv[i] << _T("'") << std::endl;
        return EXIT_FAILURE;
      }
      sampleRate = _tstoi(argv[++i]);
    } else {
      std::tcerr << _T("Unknown option '") << argv[i] << _T("'") << std::endl;
      return EXIT_FAILURE;
    }
  }

  __try { 
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return EXIT_FAILURE;
    __try {
      if (destIp) {
        send_task(sampleRate, channels, bitDepth, destIp, port);
      } else {
        recv_task(sampleRate, channels, bitDepth, port);
      }
    } __finally {
      CoUninitialize();
    }
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
