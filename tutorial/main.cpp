// GDIPlayer.cpp : 定义应用程序的入口点。
//

#include "stdafx.h"
#include "window_impl.h"
#include "scoped_ole_initializer.h"
#include "scoped_com_object.h"
#include "scoped_bitmap.h"
#include "scoped_hdc.h"
#include "scoped_selected_object.h"

#include <iostream>
#include <winuser.h>

#include <d3d9.h>
#pragma comment(lib, "D3D9.lib")

class Dispatcher : public ui::WindowDelegate {
    virtual BOOL DispatchEvent(HWND window, UINT message, WPARAM w_param, LPARAM l_param) override {
        if (message == WM_KEYDOWN && w_param == VK_ESCAPE) {
            ::DestroyWindow(window);
            return TRUE;
        }
        return FALSE;
    }
    virtual void OnWindowStateChanged(ui::WindowState new_state) override {}
};

struct CustomVertex {
    float x = 0, y = 0, z = 0, rhw = 1.0f;
    D3DCOLOR diffuse_color = 0;
    float u1 = 0, v1 = 0;
    float u2 = 0, v2 = 0;
    CustomVertex(float x, float y, float z, D3DCOLOR color, float u1, float v1, float u2, float v2)
        : x(x), y(y), z(z), diffuse_color(color), u1(u1), v1(v1), u2(u2), v2(v2) {}
};
#define D3DFVF_CUSTOM_VERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX2)

int APIENTRY wWinMain(_In_ HINSTANCE inst, _In_opt_ HINSTANCE prev_inst, _In_ LPWSTR cmd_line, _In_ int cmd_show) {
    UNREFERENCED_PARAMETER(prev_inst);
    UNREFERENCED_PARAMETER(cmd_line);

    ui::RegisterClassesAtExit();
    ScopedOleInitializer ole_initializer;

    std::unique_ptr<Dispatcher> dispatcher(new Dispatcher);
    ui::ScopedWindow window(new ui::Window(dispatcher.get()));
    window->Init(NULL, RECT{ 100, 100, 640, 480 });
    window->SetTitle(L"DirectX Tutorial");
    window->Show();

    ScopedComObject<IDirect3D9, true> direct3d(Direct3DCreate9(D3D_SDK_VERSION));
    if (!direct3d) return -1;

    D3DDISPLAYMODE mode;
    auto result = direct3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode);

    auto adapter_count = direct3d->GetAdapterCount();
    D3DADAPTER_IDENTIFIER9 adapter;
    for (UINT index = 0; index < adapter_count; ++index) {
        result = direct3d->GetAdapterIdentifier(index, 0, &adapter);
        //std::cout << L"Adapter: " << index << L"\t driver: " << adapter.Driver << L"\t Description: " << adapter.Description;
    }

    D3DPRESENT_PARAMETERS params;
    params.BackBufferWidth = 640;
    params.BackBufferHeight = 480;
    params.BackBufferFormat = D3DFMT_X8R8G8B8;
    params.BackBufferCount = 1;
    params.MultiSampleType = D3DMULTISAMPLE_NONE;
    params.MultiSampleQuality = 0;
    params.SwapEffect = D3DSWAPEFFECT_COPY;
    params.hDeviceWindow = window->hwnd();
    params.Windowed = TRUE;
    params.EnableAutoDepthStencil = FALSE;
    params.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
    params.Flags = 0;
    params.FullScreen_RefreshRateInHz = 0;
    params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    D3DDEVTYPE type = D3DDEVTYPE_HAL;
    ScopedComObject<IDirect3DDevice9> device;
    if (FAILED(direct3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window->hwnd(),
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params, device.Receive()))) {
        result = direct3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, window->hwnd(),
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params, device.Receive());
        if (FAILED(result)) {
            ::PostQuitMessage(0);
            return -2;
        }
        type = D3DDEVTYPE_REF;
    }
    device->SetRenderState(D3DRS_LIGHTING, false);

    D3DVIEWPORT9 view_port;
    view_port.X = 0;
    view_port.Y = 0;
    view_port.Width = 640;
    view_port.Height = 480;
    view_port.MinZ = 0.0f;
    view_port.MaxZ = 1.0f;
    device->SetViewport(&view_port);

    device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(128, 128, 128), 0.0f, 0);
    device->Present(nullptr, nullptr, NULL, nullptr);


    ScopedComObject<IDirect3DVertexBuffer9> vertex_buffer;
    if (FAILED(device->CreateVertexBuffer(6 * sizeof(CustomVertex), 0, D3DFVF_CUSTOM_VERTEX, D3DPOOL_SYSTEMMEM, vertex_buffer.Receive(), nullptr))) {
         return -3;
    }

    CustomVertex* vertex = nullptr;
    result = vertex_buffer->Lock(0, 0, (LPVOID*)&vertex, 0);
    vertex[0] = CustomVertex(220, 340, 0, 0xFFFFFFFF, 0, 1, 0, 1);
    vertex[1] = CustomVertex(220, 140, 0, 0xFFFFFFFF, 0, 0, 0, 0);
    vertex[2] = CustomVertex(420, 140, 0, 0xFFFFFFFF, 1, 0, 1, 0);
    vertex[3] = CustomVertex(220, 340, 0, 0xFFFFFFFF, 0, 1, 0, 1);
    vertex[4] = CustomVertex(420, 140, 0, 0xFFFFFFFF, 1, 0, 1, 0);
    vertex[5] = CustomVertex(420, 340, 0, 0xFFFFFFFF, 1, 1, 1, 1);
    result = vertex_buffer->Unlock();

    //ScopedComObject<IDirect3DIndexBuffer9> index_buffer;
    ScopedBitmap bitmap(::LoadImage(NULL, L"test.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE));
    ScopedComObject<IDirect3DSurface9> offline_surface;
    if (FAILED(device->CreateOffscreenPlainSurface(bitmap.width(), bitmap.height(), D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, offline_surface.Receive(), nullptr))) {
        return -4;
    }
    D3DLOCKED_RECT rect;
    if (SUCCEEDED(offline_surface->LockRect(&rect, nullptr, 0))) {
        auto buffer = static_cast<BYTE*>(rect.pBits);
        ScopedCreateDC dc(::CreateCompatibleDC(nullptr));
        ScopedSelectedBitmap last_bitmap(dc.Get(), bitmap.Get());
        for (int h = 0; h < bitmap.height(); ++h) {
            for (int w = 0; w < bitmap.width(); ++w) {
                auto color = ::GetPixel(dc.Get(), w, h);
                DWORD red = GetRValue(color);
                DWORD green = GetGValue(color);
                DWORD blue = GetBValue(color);
                DWORD dwColor = (red << 16) | (green << 8) | blue;
                DWORD pixel = h * rect.Pitch + w * 4;
                std::memcpy(&buffer[pixel], &dwColor, 4);
            }
        }
        result = offline_surface->UnlockRect();
    }
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);

    ScopedComObject<IDirect3DTexture9> texture;
    if (FAILED(device->CreateTexture(bitmap.width(), bitmap.height(), 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, texture.Receive(), nullptr))) {
        return -5;
    } else {
        ScopedComObject<IDirect3DSurface9> surface;
        texture->GetSurfaceLevel(0, surface.Receive());
        result = device->UpdateSurface(offline_surface.get(), nullptr, surface.get(), nullptr);
        if (FAILED(result)) {
            auto code = ::GetLastError();
            return -6;
        }
        device->SetTexture(0, texture.Detach());
    }

    window->FireEvent([&](HWND window, UINT message, WPARAM w_param, LPARAM l_param)->BOOL {
        if (message == WM_PAINT) {
            device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 255, 255, 255), 0.0f, 0);
            device->SetStreamSource(0, vertex_buffer, 0, sizeof(CustomVertex));
            device->SetFVF(D3DFVF_CUSTOM_VERTEX);
            device->BeginScene();
            device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
            device->EndScene();
            device->Present(0, 0, 0, 0);
        } else if (message == WM_KEYDOWN && w_param == VK_SPACE) {
            return FALSE;
            params.Windowed = !params.Windowed;
            if (params.Windowed) {
                params.BackBufferWidth = 640;
                params.BackBufferHeight = 480;
            } else {
                params.BackBufferWidth = mode.Width;
                params.BackBufferHeight = mode.Height;
            }
            result = device->Reset(&params);
        }
        return FALSE;
    });

    ui::RunLoop();
    return 0;
}