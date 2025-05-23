#include <atlbase.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>

#pragma comment(lib, "ole32.lib")

namespace {

class Audio {
private:
  Audio() noexcept {}

  static TCHAR* tcscpy(TCHAR* dest, const TCHAR* src) noexcept {
    TCHAR* beg = dest; while ((*dest++ = *src++) != 0); return beg;
  }

  static UINT tcslen(const TCHAR* str) noexcept {
    const TCHAR* p = str; while (*p != 0) ++p; return static_cast<UINT>(p - str);
  }

public:
  static Audio* create() {
    return new Audio();
  }

  class Device {
  public:
    template <UINT CharacterCount>
    class Names {
    public:
      class Iter {
      private:
        const TCHAR* buffer_;

        static const TCHAR* findNext(const TCHAR* buffer) noexcept {
          buffer += tcslen(buffer) + 1;
          return *buffer != 0 ? buffer : nullptr;
        }

      public:
        Iter(const TCHAR* buffer) noexcept
          : buffer_{buffer} {}

        const TCHAR* operator*() const noexcept {
          return buffer_;
        }

        bool operator!=(const Iter& rhs) const noexcept {
          return buffer_ != rhs.buffer_;
        }

        Iter& operator++() noexcept {
          buffer_ = buffer_ ? findNext(buffer_) : nullptr;
          return *this;
        }
      };

      static Names<CharacterCount>* create() {
        return new Names<CharacterCount>();
      }

      Names(const Names<CharacterCount>&) noexcept = delete;

      Names(Names<CharacterCount>&&) noexcept = delete;

      const TCHAR* append(const TCHAR* str) noexcept {
        TCHAR* buffer = buffer_;
        while (*buffer != 0) {
          buffer += tcslen(buffer) + 1;
        };
        if (buffer + tcslen(str) + 1 >= buffer_ + CharacterCount) {
          return nullptr;
        }
        return tcscpy(buffer, str);
      }

      Iter begin() const noexcept {
        return {buffer_};
      }

      Iter end() const noexcept {
        return {nullptr};
      }

      template <typename F>
      void forEach(F func) const noexcept(noexcept(func(static_cast<const TCHAR*>(nullptr)))) {
        for (Iter iter = begin(); iter != end(); ++iter) {
          func(*iter);
        }
      }

    private:
      Names() noexcept
        : buffer_{} {}

      TCHAR buffer_[CharacterCount];
    };
  };

  class Render {
  private:
    friend class Audio;

    HANDLE hEvent_;
    WAVEFORMATEX waveFormat_;
    IMMDevice* pDevice_;
    IAudioClient* pClient_;
    IAudioRenderClient* pRender_;

    Render(IMMDevice* pDevice, IAudioClient* pClient, IAudioRenderClient* pRender, HANDLE hEvent, const WAVEFORMATEX& waveFormat) noexcept 
      : hEvent_(hEvent)
      , waveFormat_(waveFormat)
      , pDevice_(pDevice)
      , pClient_(pClient)
      , pRender_(pRender) {}

  public:
    ~Render() noexcept {
      pRender_->Release();
      pClient_->Release();  
      pDevice_->Release();
      CloseHandle(hEvent_);
    }

    IAudioClient* client() const noexcept {
      return pClient_;
    }

    IAudioRenderClient* renderClient() const noexcept {
      return pRender_;
    }

    HANDLE getEvent() const noexcept {
      return hEvent_;
    }

    const WAVEFORMATEX& getFormat() const noexcept {
        return waveFormat_;
    }
  };

  class Capture {
  private:
    friend class Audio;

    HANDLE hEvent_;
    WAVEFORMATEX waveFormat_;
    IMMDevice* pDevice_;
    IAudioClient* pClient_;
    IAudioCaptureClient* pCapture_;

    Capture(IMMDevice* pDevice, IAudioClient* pClient, IAudioCaptureClient* pCapture, HANDLE hEvent, const WAVEFORMATEX& waveFormat) noexcept 
      : hEvent_(hEvent)
      , waveFormat_(waveFormat)
      , pDevice_(pDevice)
      , pClient_(pClient)
      , pCapture_(pCapture) {}

  public:
    ~Capture() noexcept {
      pCapture_->Release();
      pClient_->Release();  
      pDevice_->Release();
      CloseHandle(hEvent_);
    }

    IAudioClient* client() const noexcept {
      return pClient_;
    }

    IAudioCaptureClient* captureClient() const noexcept {
      return pCapture_;
    }

    HANDLE getEvent() const noexcept {
      return hEvent_;
    }

    const WAVEFORMATEX& getFormat() const noexcept {
      return waveFormat_;
    }
  };

  template <UINT CharacterCount>
  bool initRenderNames(Device::Names<CharacterCount>& names, const TCHAR* defaultName = _T("Unrecognized Device")) noexcept {
    CComPtr<IMMDeviceEnumerator> pEnum;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pEnum)))) return false;

    CComPtr<IMMDeviceCollection> pCollection;
    if (FAILED(pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection))) return false;

    UINT count;
    if (FAILED(pCollection->GetCount(&count))) return false;
    for (UINT i = 0; i < count; ++i) {
      CComPtr<IMMDevice> pDevice;
      if (SUCCEEDED(pCollection->Item(i, &pDevice))) {
        CComPtr<IPropertyStore> pProperties;
        if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProperties))) {
          PROPVARIANT varName;
          PropVariantInit(&varName);
          if (SUCCEEDED(pProperties->GetValue(PKEY_Device_FriendlyName, &varName))) {
            if (!names.append(varName.pwszVal)) return false;
            continue;
          }
        }
      }
      names.append(defaultName);
    }
    return true;
  }

  template <UINT CharacterCount>
  bool initCaptureNames(Device::Names<CharacterCount>& names, const TCHAR* defaultName = _T("Unrecognized Device")) noexcept {
    CComPtr<IMMDeviceEnumerator> pEnum;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pEnum)))) return false;

    CComPtr<IMMDeviceCollection> pCollection;
    if (FAILED(pEnum->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCollection))) return false;

    UINT count;
    if (FAILED(pCollection->GetCount(&count))) return false;
    for (UINT i = 0; i < count; ++i) {
      CComPtr<IMMDevice> pDevice;
      if (SUCCEEDED(pCollection->Item(i, &pDevice))) {
        CComPtr<IPropertyStore> pProperties;
        if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProperties))) {
          PROPVARIANT varName;
          PropVariantInit(&varName);
          if (SUCCEEDED(pProperties->GetValue(PKEY_Device_FriendlyName, &varName))) {
            if (!names.append(varName.pwszVal)) return false;
            continue;
          }
        }
      }
      names.append(defaultName);
    }
    return true;
  }

  Render* createRenderDevice(int deviceIndex, int channels, int sampleRate, int bitDepth) {
    CComPtr<IMMDeviceEnumerator> pEnum;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pEnum)))) return nullptr;
    CComPtr<IMMDeviceCollection> pCollection;
    if (FAILED(pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection))) return nullptr;
    CComPtr<IMMDevice> pDevice;
    if (FAILED(pCollection->Item(deviceIndex, &pDevice))) return nullptr;
    CComPtr<IAudioClient> pClient;
    if (FAILED(pDevice->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, nullptr, reinterpret_cast<void**>(&pClient)))) return nullptr;

    WAVEFORMATEX waveFormat{};
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = channels;
    waveFormat.nSamplesPerSec = sampleRate;
    waveFormat.wBitsPerSample = bitDepth;
    waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize = 0;

    REFERENCE_TIME bufferDuration = 1000000; // 100ms
    if (FAILED(pClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, bufferDuration, 0, &waveFormat, nullptr))) return nullptr;

    HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (hEvent == nullptr || FAILED(pClient->SetEventHandle(hEvent))) return nullptr;

    CComPtr<IAudioRenderClient> pRender;
    if (FAILED(pClient->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&pRender)))) return nullptr;

    return new Render(pDevice.Detach(), pClient.Detach(), pRender.Detach(), hEvent, waveFormat);
  }

  Capture* createCaptureDevice(int deviceIndex, int channels, int sampleRate, int bitDepth) {
    CComPtr<IMMDeviceEnumerator> pEnum;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pEnum)))) return nullptr;
    CComPtr<IMMDeviceCollection> pCollection;
    if (FAILED(pEnum->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCollection))) return nullptr;
    CComPtr<IMMDevice> pDevice;
    if (FAILED(pCollection->Item(deviceIndex, &pDevice))) return nullptr;
    CComPtr<IAudioClient> pClient;
    if (FAILED(pDevice->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, nullptr, reinterpret_cast<void**>(&pClient)))) return nullptr;

    WAVEFORMATEX waveFormat{};
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = channels;
    waveFormat.nSamplesPerSec = sampleRate;
    waveFormat.wBitsPerSample = bitDepth;
    waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    waveFormat.cbSize = 0;

    if (FAILED(pClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, &waveFormat, nullptr))) return nullptr;

    HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (hEvent == nullptr || FAILED(pClient->SetEventHandle(hEvent))) return nullptr;

    CComPtr<IAudioCaptureClient> pCapture;
    if (FAILED(pClient->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&pCapture)))) return nullptr;

    return new Capture(pDevice.Detach(), pClient.Detach(), pCapture.Detach(), hEvent, waveFormat);
  }

};

} // namespace
