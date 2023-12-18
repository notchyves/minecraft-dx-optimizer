#include <Windows.h>
#include <thread>
#include "d2d1.h"
#include <d2d1.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d2d1_3.h>
#include <d3d11.h>
#include <dwrite_1.h>
#include <winerror.h>
#include "Include/MinHook.h"
#include "Include/kiero/kiero.h"

bool init = false;
bool init2 = false;
ID3D11Device* d3d11Device;
ID3D12Device5* d3d12Device;
IDXGISurface* back_buffer = nullptr;
ID2D1DeviceContext* context = nullptr;
ID2D1SolidColorBrush* brush = nullptr;

wchar_t font[32] = L"Segoe UI";
IDWriteFactory* write_factory = nullptr;
IDWriteTextFormat* text_format = nullptr;

typedef HRESULT(__thiscall* present_t)(IDXGISwapChain3*, UINT, UINT);
present_t original_present;

typedef HRESULT(__thiscall* resize_buffers_t)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
resize_buffers_t original_resize_buffers;

int frames = 0;
float fps = 0.f;

static std::chrono::high_resolution_clock fpsclock;
static std::chrono::steady_clock::time_point start = fpsclock.now();
static std::chrono::steady_clock::time_point previousFrameTime = fpsclock.now();

template<typename T>
static void Release(T*& pPtr)
{
    if (pPtr != nullptr)
    {
        pPtr->Release();
        pPtr = nullptr;
    }
};

HRESULT present_callback(IDXGISwapChain3* swap_chain, UINT sync_interval, UINT flags)
{

    std::chrono::duration<float> elapsed = fpsclock.now() - start;
    frames += 1;

    if (elapsed.count() >= 0.5f) {
        fps = static_cast<int>(frames / elapsed.count());
        frames = 0;
        start = fpsclock.now();
    }

    if (!init)
    {
        if (SUCCEEDED(swap_chain->GetDevice(IID_PPV_ARGS(&d3d12Device))))
        { 
            d3d12Device->RemoveDevice();
            init = true;
            return original_present(swap_chain, sync_interval, DXGI_PRESENT_DO_NOT_WAIT); 
        }
    }

    if (FAILED(swap_chain->GetDevice(IID_PPV_ARGS(&d3d11Device))))
        return original_present(swap_chain, sync_interval, DXGI_PRESENT_DO_NOT_WAIT);
    
    if (!init2)
    {
        if (SUCCEEDED(swap_chain->GetDevice(IID_PPV_ARGS(&d3d11Device))))
        {

            swap_chain->GetDevice(IID_PPV_ARGS(&d3d11Device));
            swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
            
            const D2D1_CREATION_PROPERTIES properties = D2D1::CreationProperties(
                D2D1_THREADING_MODE_SINGLE_THREADED,
                D2D1_DEBUG_LEVEL_NONE,
                D2D1_DEVICE_CONTEXT_OPTIONS_NONE
            );

            D2D1CreateDeviceContext(back_buffer, properties, &context);

            Release(back_buffer);

            DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&write_factory);

            init2 = true;
        }
    }


    context->BeginDraw();

    write_factory->CreateTextFormat(font, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 24.0f, L"en-us", &text_format);
    text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    
    D2D1_COLOR_F color = D2D1::ColorF(D2D1::ColorF::LightSlateGray, 0.35f);
    wchar_t fpsText[16];
    swprintf_s(fpsText, L"%.2f", fps);

    context->CreateSolidColorBrush(color, &brush);
    context->DrawTextA(fpsText, wcslen(fpsText), text_format, 
        D2D1::RectF(10, 10, 10000, 10000), brush);

    Release(brush);
    Release(text_format);

    context->EndDraw();

    return original_present(swap_chain, sync_interval, DXGI_PRESENT_DO_NOT_WAIT); // disable vsync lmao
}

HRESULT resize_buffers_callback(IDXGISwapChain3* swap_chain, UINT buffer_count, UINT width, UINT height, DXGI_FORMAT new_format, UINT swap_chain_flags)
{
    Release(write_factory);
    Release(context);
    Release(d3d11Device);

    init2 = false;

    return original_resize_buffers(swap_chain, buffer_count, width, height, new_format, swap_chain_flags);
}

void Inject()
{
    // halal optifine start
    if (kiero::init(kiero::RenderType::D3D12) == kiero::Status::Success)
    {
        kiero::bind(140, (void**)&original_present, present_callback);
        kiero::bind(145, (void**)&original_resize_buffers, resize_buffers_callback);
        return;
    }
    if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success)
    {
        kiero::bind(8, (void**)&original_present, present_callback);
        kiero::bind(13, (void**)&original_resize_buffers, resize_buffers_callback);
        return;
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Inject, hModule, 0, 0);
            break;
        case DLL_PROCESS_DETACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }

    return TRUE;
}