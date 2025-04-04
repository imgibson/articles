#include <atlbase.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace {

class MediaFoundation {
private:
  MediaFoundation() noexcept {}

  template <typename T>
  struct CComPtrArray {
    T** array;
    UINT32 count;

    CComPtrArray() noexcept
      : array(nullptr)
      , count(0) {}

    CComPtrArray(const CComPtrArray&) noexcept = delete;

    CComPtrArray(CComPtrArray&&) noexcept = delete;

    ~CComPtrArray() {
      if (array) {
        for (decltype(count) i = 0; i < count; ++i) {
          if (array[i]) {
            array[i]->Release();
          }
        }
        CoTaskMemFree(array);
      }
    }
  };

public:
  static MediaFoundation* create() {
    return SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE)) ? new MediaFoundation() : nullptr;
  }

  MediaFoundation(const MediaFoundation&) noexcept = delete;

  MediaFoundation(MediaFoundation&&) noexcept = delete;

  ~MediaFoundation() {
    MFShutdown();
  }

  class Camera {
  private:
    friend class MediaFoundation;

    CComPtr<IMFSourceReader> reader_;
    UINT width_;
    UINT height_;

    Camera(IMFSourceReader* reader, UINT width, UINT height) noexcept
      : reader_()
      , width_(width)
      , height_(height) {
      reader_.Attach(reader);
    }

  public:
    Camera(const Camera&) noexcept = delete;

    Camera(Camera&&) noexcept = delete;

    UINT getWidth() const noexcept {
      return width_;
    }

    UINT getHeight() const noexcept {
      return height_;
    }

    HBITMAP grab() noexcept {
      DWORD dwFlags;
      DWORD dwStreamIndex;
      LONGLONG llTimeStamp;

      CComPtr<IMFSample> pSample;
      if (FAILED(reader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &dwStreamIndex, &dwFlags, &llTimeStamp, &pSample)) || !pSample) return nullptr;

      CComPtr<IMFMediaBuffer> pBuffer;
      if (FAILED(pSample->ConvertToContiguousBuffer(&pBuffer))) return nullptr;

      BYTE* pData = nullptr;
      DWORD maxLen = 0;
      DWORD currLen = 0;
      if (FAILED(pBuffer->Lock(&pData, &maxLen, &currLen))) return nullptr;
      struct UnlockScope {
        ~UnlockScope() {
          pBuffer->Unlock();
        }
        IMFMediaBuffer* pBuffer;
      } unlock{pBuffer};

      BITMAPINFO bmi = {0};
      bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bmi.bmiHeader.biWidth = width_;
      bmi.bmiHeader.biHeight = -static_cast<decltype(bmi.bmiHeader.biHeight)>(height_);
      bmi.bmiHeader.biPlanes = 1;
      bmi.bmiHeader.biBitCount = 32;
      bmi.bmiHeader.biCompression = BI_RGB;

      HDC hdc = GetDC(nullptr);
      if (!hdc) return nullptr;
      struct HDCScope {
        ~HDCScope() {
          ReleaseDC(nullptr, hdc);
        }
        HDC hdc;
      } releaseDC{hdc};

      HDC hMemDC = CreateCompatibleDC(hdc);
      if (!hMemDC) return nullptr;
      struct HMemDCScope {
        ~HMemDCScope() {
          DeleteDC(hMemDC);
        }
        HDC hMemDC;
      } deleteDC{hMemDC};

      BYTE* imageData = static_cast<BYTE*>(malloc(width_ * height_ * 4));
      if (!imageData) return nullptr;
      struct Image {
        ~Image() {
          free(data);
        }
        UINT width;
        UINT height;
        BYTE* data;
      } image{width_, height_, imageData};

      for (UINT i = 0; i < image.width * image.height; ++i) {
        float y = pData[(i * 2)];
        float u = pData[(i / 2) * 4 + 1] - 128.0f;
        float v = pData[(i / 2) * 4 + 3] - 128.0f;
        y = (y <= 16.0f) ? 0.0f : (y <= 239.0f) ? (y - 16.0f) * 1.164f : (239.0f - 16.0f) * 1.164f;
        image.data[(i * 4) + 0] = static_cast<BYTE>(max(0.0f, min(255.0f, y + 1.976f * u)));              // B
        image.data[(i * 4) + 1] = static_cast<BYTE>(max(0.0f, min(255.0f, y - 0.383f * u - 0.813f * v))); // G
        image.data[(i * 4) + 2] = static_cast<BYTE>(max(0.0f, min(255.0f, y + 1.596f * v)));              // R
        image.data[(i * 4) + 3] = 255; // Alpha channel set to 255 (opaque)
      }

      return CreateDIBitmap(hdc, &bmi.bmiHeader, CBM_INIT, image.data, &bmi, DIB_RGB_COLORS);
    }

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

    private:
      static TCHAR* tcscpy(TCHAR* dest, const TCHAR* src) noexcept {
        TCHAR* beg = dest; while ((*dest++ = *src++) != 0); return beg;
      }

      static UINT tcslen(const TCHAR* str) noexcept {
        const TCHAR* p = str; while (*p != 0) ++p; return static_cast<UINT>(p - str);
      }

      Names() noexcept
        : buffer_{} {}

      TCHAR buffer_[CharacterCount];
    };
  };

  template <UINT CharacterCount>
  bool initNames(Camera::Names<CharacterCount>& names, const TCHAR* defaultName) noexcept {
    CComPtr<IMFAttributes> pAttributes;
    CComPtrArray<IMFActivate> ppDevices;
    if (FAILED(MFCreateAttributes(&pAttributes, 1))) return false;
    if (FAILED(pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))) return false;
    if (FAILED(MFEnumDeviceSources(pAttributes, &ppDevices.array, &ppDevices.count))) return false;
    
    for (decltype(ppDevices.count) i = 0; i < ppDevices.count; ++i) {
      WCHAR* pName = nullptr;
      UINT32 cchName = 0;
      if (SUCCEEDED(ppDevices.array[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &pName, &cchName))) {
        names.append(pName);
        CoTaskMemFree(pName);
        continue;
      }
      names.append(defaultName);
    }
    return true;
  }

  Camera* createCamera(int index) {
    CComPtr<IMFAttributes> pAttributes;
    CComPtrArray<IMFActivate> ppDevices;
    if (FAILED(MFCreateAttributes(&pAttributes, 1))) return nullptr;
    if (FAILED(pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))) return nullptr;
    if (FAILED(MFEnumDeviceSources(pAttributes, &ppDevices.array, &ppDevices.count))) return nullptr;

    CComPtr<IMFMediaSource> pMediaSource;
    if (FAILED(ppDevices.array[index]->ActivateObject(IID_IMFMediaSource, reinterpret_cast<void**>(&pMediaSource)))) return nullptr;

    CComPtr<IMFSourceReader> pReader;
    if (FAILED(MFCreateSourceReaderFromMediaSource(pMediaSource, pAttributes, &pReader))) return nullptr;
    if (FAILED(pReader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE))) return nullptr;

    CComPtr<IMFMediaType> pNativeType;
    if (FAILED(pReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &pNativeType))) return nullptr;

    UINT32 width = 0, height = 0;
    if (FAILED(MFGetAttributeSize(pNativeType, MF_MT_FRAME_SIZE, &width, &height))) return nullptr;

    return new Camera(pReader.Detach(), width, height);
  }
};

} // namespace
