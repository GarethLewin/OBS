/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/


#include "GraphicsCaptureHook.h"

#include <D3D10_1.h>


typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef signed char GLbyte;
typedef short GLshort;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned long GLulong;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;
typedef ptrdiff_t GLintptrARB;
typedef ptrdiff_t GLsizeiptrARB;

#define GL_FRONT 0x0404
#define GL_BACK 0x0405

#define GL_UNSIGNED_BYTE 0x1401

#define GL_RGB 0x1907
#define GL_RGBA 0x1908

#define GL_BGR 0x80E0
#define GL_BGRA 0x80E1

#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601

#define GL_READ_ONLY 0x88B8
#define GL_WRITE_ONLY 0x88B9
#define GL_READ_WRITE 0x88BA
#define GL_BUFFER_ACCESS 0x88BB
#define GL_BUFFER_MAPPED 0x88BC
#define GL_BUFFER_MAP_POINTER 0x88BD
#define GL_STREAM_DRAW 0x88E0
#define GL_STREAM_READ 0x88E1
#define GL_STREAM_COPY 0x88E2
#define GL_STATIC_DRAW 0x88E4
#define GL_STATIC_READ 0x88E5
#define GL_STATIC_COPY 0x88E6
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_DYNAMIC_READ 0x88E9
#define GL_DYNAMIC_COPY 0x88EA
#define GL_PIXEL_PACK_BUFFER 0x88EB
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#define GL_PIXEL_PACK_BUFFER_BINDING 0x88ED
#define GL_PIXEL_UNPACK_BUFFER_BINDING 0x88EF

#define GL_TEXTURE_2D 0x0DE1

//------------------------------------------------

typedef void   (WINAPI *GLREADBUFFERPROC)(GLenum);
typedef void   (WINAPI *GLDRAWBUFFERPROC)(GLenum mode);
typedef void   (WINAPI *GLREADPIXELSPROC)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLvoid*);
typedef GLenum (WINAPI *GLGETERRORPROC)();
typedef BOOL   (WINAPI *WGLSWAPLAYERBUFFERSPROC)(HDC, UINT);
typedef BOOL   (WINAPI *WGLSWAPBUFFERSPROC)(HDC);
typedef BOOL   (WINAPI *WGLDELETECONTEXTPROC)(HGLRC);
typedef PROC   (WINAPI *WGLGETPROCADDRESSPROC)(LPCSTR);
typedef BOOL   (WINAPI *WGLMAKECURRENTPROC)(HDC, HGLRC);
typedef HGLRC  (WINAPI *WGLCREATECONTEXTPROC)(HDC);

GLREADBUFFERPROC        glReadBuffer          = NULL;
GLDRAWBUFFERPROC        glDrawBuffer          = NULL;
GLREADPIXELSPROC        glReadPixels          = NULL;
GLGETERRORPROC          glGetError            = NULL;
WGLSWAPLAYERBUFFERSPROC jimglSwapLayerBuffers = NULL;
WGLSWAPBUFFERSPROC      jimglSwapBuffers      = NULL;
WGLDELETECONTEXTPROC    jimglDeleteContext    = NULL;
WGLGETPROCADDRESSPROC   jimglGetProcAddress   = NULL;
WGLMAKECURRENTPROC      jimglMakeCurrent      = NULL;
WGLCREATECONTEXTPROC    jimglCreateContext    = NULL;

//------------------------------------------------

typedef void      (WINAPI *GLBUFFERDATAARBPROC) (GLenum target, GLsizeiptrARB size, const GLvoid* data, GLenum usage);
typedef void      (WINAPI *GLDELETEBUFFERSARBPROC)(GLsizei n, const GLuint* buffers);
typedef void      (WINAPI *GLDELETETEXTURESPROC)(GLsizei n, const GLuint* buffers);
typedef void      (WINAPI *GLGENBUFFERSARBPROC)(GLsizei n, GLuint* buffers);
typedef void      (WINAPI *GLGENTEXTURESPROC)(GLsizei n, GLuint* textures);
typedef GLvoid*   (WINAPI *GLMAPBUFFERPROC)(GLenum target, GLenum access);
typedef GLboolean (WINAPI *GLUNMAPBUFFERPROC)(GLenum target);
typedef void      (WINAPI *GLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void      (WINAPI *GLBINDTEXTUREPROC)(GLenum target, GLuint texture);

GLBUFFERDATAARBPROC     glBufferData       = NULL;
GLDELETEBUFFERSARBPROC  glDeleteBuffers    = NULL;
GLDELETETEXTURESPROC    glDeleteTextures   = NULL;
GLGENBUFFERSARBPROC     glGenBuffers       = NULL;
GLGENTEXTURESPROC       glGenTextures      = NULL;
GLMAPBUFFERPROC         glMapBuffer        = NULL;
GLUNMAPBUFFERPROC       glUnmapBuffer      = NULL;
GLBINDBUFFERPROC        glBindBuffer       = NULL;
GLBINDTEXTUREPROC       glBindTexture      = NULL;

//------------------------------------------------

HookData                glHookSwapBuffers;
HookData                glHookSwapLayerBuffers;
HookData                glHookwglSwapBuffers;
HookData                glHookDeleteContext;

//2 buffers turns out to be more optimal than 3 -- seems that cache has something to do with this in GL
#define                 NUM_BUFFERS 2
#define                 ZERO_ARRAY {0, 0}

GLuint                  gltextures[NUM_BUFFERS] = ZERO_ARRAY;
HDC                     hdcAcquiredDC = NULL;
HWND                    hwndTarget = NULL;

extern HANDLE           hCopyThread;
extern HANDLE           hCopyEvent;
extern bool             bKillThread;
HANDLE                  glDataMutexes[NUM_BUFFERS];
extern void             *pCopyData;
extern DWORD            curCPUTexture;

bool                    glLockedTextures[NUM_BUFFERS];
extern MemoryCopyData   *copyData;
extern LPBYTE           textureBuffers[2];
extern DWORD            curCapture;
extern BOOL             bHasTextures;
extern DWORD            copyWait;
extern LONGLONG         lastTime;

CaptureInfo             glcaptureInfo;

//------------------------------------------------
// nvidia specific (but also works on AMD surprisingly)

bool bNVCaptureAvailable = false;
bool bFBOAvailable = false;

typedef BOOL   (WINAPI *WGLSETRESOURCESHAREHANDLENVPROC)(void*, HANDLE);
typedef HANDLE (WINAPI *WGLDXOPENDEVICENVPROC)(void*);
typedef BOOL   (WINAPI *WGLDXCLOSEDEVICENVPROC)(HANDLE);
typedef HANDLE (WINAPI *WGLDXREGISTEROBJECTNVPROC)(HANDLE, void *, GLuint, GLenum, GLenum);
typedef BOOL   (WINAPI *WGLDXUNREGISTEROBJECTNVPROC)(HANDLE, HANDLE);
typedef BOOL   (WINAPI *WGLDXOBJECTACCESSNVPROC)(HANDLE, GLenum);
typedef BOOL   (WINAPI *WGLDXLOCKOBJECTSNVPROC)(HANDLE, GLint, HANDLE *);
typedef BOOL   (WINAPI *WGLDXUNLOCKOBJECTSNVPROC)(HANDLE, GLint, HANDLE *);

WGLSETRESOURCESHAREHANDLENVPROC wglDXSetResourceShareHandleNV = NULL;
WGLDXOPENDEVICENVPROC           wglDXOpenDeviceNV             = NULL;
WGLDXCLOSEDEVICENVPROC          wglDXCloseDeviceNV            = NULL;
WGLDXREGISTEROBJECTNVPROC       wglDXRegisterObjectNV         = NULL;
WGLDXUNREGISTEROBJECTNVPROC     wglDXUnregisterObjectNV       = NULL;
WGLDXOBJECTACCESSNVPROC         wglDXObjectAccessNV           = NULL;
WGLDXLOCKOBJECTSNVPROC          wglDXLockObjectsNV            = NULL;
WGLDXUNLOCKOBJECTSNVPROC        wglDXUnlockObjectsNV          = NULL;

#define WGL_ACCESS_READ_ONLY_NV             0x0000
#define WGL_ACCESS_READ_WRITE_NV            0x0001
#define WGL_ACCESS_WRITE_DISCARD_NV         0x0002

HANDLE gl_handle = NULL;
HANDLE gl_dxDevice = NULL;

//------------------------------------------------

typedef void (WINAPI *GLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint* buffers);
typedef void (WINAPI *GLDELETEFRAMEBUFFERSPROC)(GLsizei n, GLuint *framebuffers);
typedef void (WINAPI *GLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void (WINAPI *GLBLITFRAMEBUFFERPROC)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void (WINAPI *GLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);

GLGENFRAMEBUFFERSPROC      glGenFramebuffers      = NULL;
GLDELETEFRAMEBUFFERSPROC   glDeleteFramebuffers   = NULL;
GLBINDFRAMEBUFFERPROC      glBindFramebuffer      = NULL;
GLBLITFRAMEBUFFERPROC      glBlitFramebuffer      = NULL;
GLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = NULL;

#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_COLOR_ATTACHMENT1 0x8CE1

extern BOOL             bUseSharedTextures;
extern SharedTexData    *texData;
extern ID3D10Device1    *shareDevice;
extern ID3D10Resource   *copyTextureIntermediary;
extern HANDLE           sharedHandle;

GLuint gl_fbo       = 0;
GLuint gl_sharedtex = 0;

extern HMODULE          hD3D9Dll;

extern bool             bDXGIHooked;


void ClearGLData()
{
    if(copyData)
        copyData->lastRendered = -1;

    if(hCopyThread)
    {
        bKillThread = true;
        SetEvent(hCopyEvent);
        if(WaitForSingleObject(hCopyThread, 500) != WAIT_OBJECT_0)
            TerminateThread(hCopyThread, -1);

        CloseHandle(hCopyThread);
        CloseHandle(hCopyEvent);

        hCopyThread = NULL;
        hCopyEvent = NULL;
    }

    for(int i=0; i<NUM_BUFFERS; i++)
    {
        if(glLockedTextures[i])
        {
            OSEnterMutex(glDataMutexes[i]);

            glBindBuffer(GL_PIXEL_PACK_BUFFER, gltextures[i]);
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

            glLockedTextures[i] = false;

            OSLeaveMutex(glDataMutexes[i]);
        }
    }

    if (bUseSharedTextures) {
        if (gl_dxDevice) {
            if (gl_handle) {
                wglDXUnregisterObjectNV(gl_dxDevice, gl_handle);
                gl_handle = NULL;
            }

            wglDXCloseDeviceNV(gl_dxDevice);
            gl_dxDevice = NULL;
        }

        if (gl_sharedtex) {
            glDeleteTextures(1, &gl_sharedtex);
            gl_sharedtex = 0;
        }

        if (gl_fbo) {
            glDeleteFramebuffers(1, &gl_fbo);
            gl_fbo = 0;
        }

        SafeRelease(copyTextureIntermediary);
        SafeRelease(shareDevice);
    } else if(bHasTextures) {
        glDeleteBuffers(NUM_BUFFERS, gltextures);
        ZeroMemory(gltextures, sizeof(gltextures));
    }

    for(int i=0; i<NUM_BUFFERS; i++)
    {
        if(glDataMutexes[i])
        {
            OSCloseMutex(glDataMutexes[i]);
            glDataMutexes[i] = NULL;
        }
    }

    DestroySharedMemory();
    bHasTextures = false;
    texData = NULL;
    copyData = NULL;
    copyWait = 0;
    lastTime = 0;
    curCapture = 0;
    curCPUTexture = 0;
    keepAliveTime = 0;
    resetCount++;
    pCopyData = NULL;

    logOutput << CurrentTimeString() << "---------------------- Cleared OpenGL Capture ----------------------" << endl;
}

DWORD CopyGLCPUTextureThread(LPVOID lpUseless);

typedef HRESULT (WINAPI *CREATEDXGIFACTORY1PROC)(REFIID riid, void **ppFactory);

void DoGLGPUHook(RECT &rc)
{
    bUseSharedTextures = true;
    glcaptureInfo.cx = rc.right;
    glcaptureInfo.cy = rc.bottom;

    BOOL bSuccess = false;

    bDXGIHooked = true;

    HRESULT hErr;

    HMODULE hD3D10_1 = LoadLibrary(TEXT("d3d10_1.dll"));
    if(!hD3D10_1)
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoGLGPUHook: Could not load D3D10.1" << endl;
        goto finishGPUHook;
    }

    HMODULE hDXGI = LoadLibrary(TEXT("dxgi.dll"));
    if(!hDXGI)
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoGLGPUHook: Could not load dxgi" << endl;
        goto finishGPUHook;
    }

    CREATEDXGIFACTORY1PROC createDXGIFactory1 = (CREATEDXGIFACTORY1PROC)GetProcAddress(hDXGI, "CreateDXGIFactory1");
    if(!createDXGIFactory1)
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoGLGPUHook: Could not load 'CreateDXGIFactory1'" << endl;
        goto finishGPUHook;
    }

    PFN_D3D10_CREATE_DEVICE1 d3d10CreateDevice1 = (PFN_D3D10_CREATE_DEVICE1)GetProcAddress(hD3D10_1, "D3D10CreateDevice1");
    if(!d3d10CreateDevice1)
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoGLGPUHook: Could not load 'D3D10CreateDevice1'" << endl;
        goto finishGPUHook;
    }

    IDXGIFactory1 *factory;
    if(FAILED(hErr = (*createDXGIFactory1)(__uuidof(IDXGIFactory1), (void**)&factory)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoD3D9GPUHook: CreateDXGIFactory1 failed, result = " << (UINT)hErr << endl;
        goto finishGPUHook;
    }

    IDXGIAdapter1 *adapter;
    if(FAILED(hErr = factory->EnumAdapters1(0, &adapter)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoGLGPUHook: factory->EnumAdapters1 failed, result = " << (UINT)hErr << endl;
        factory->Release();
        goto finishGPUHook;
    }

    if(FAILED(hErr = (*d3d10CreateDevice1)(adapter, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, &shareDevice)))
    {
        if(FAILED(hErr = (*d3d10CreateDevice1)(adapter, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_FEATURE_LEVEL_9_3, D3D10_1_SDK_VERSION, &shareDevice)))
        {
            RUNEVERYRESET logOutput << CurrentTimeString() << "DoGLGPUHook: Could not create D3D10.1 device, result = " << (UINT)hErr << endl;
            adapter->Release();
            factory->Release();
            goto finishGPUHook;
        }
    }

    adapter->Release();
    factory->Release();

    //------------------------------------------------

    D3D10_TEXTURE2D_DESC texGameDesc;
    ZeroMemory(&texGameDesc, sizeof(texGameDesc));
    texGameDesc.Width               = glcaptureInfo.cx;
    texGameDesc.Height              = glcaptureInfo.cy;
    texGameDesc.MipLevels           = 1;
    texGameDesc.ArraySize           = 1;
    texGameDesc.Format              = DXGI_FORMAT_B8G8R8X8_UNORM;
    texGameDesc.SampleDesc.Count    = 1;
    texGameDesc.BindFlags           = D3D10_BIND_RENDER_TARGET|D3D10_BIND_SHADER_RESOURCE;
    texGameDesc.Usage               = D3D10_USAGE_DEFAULT;
    texGameDesc.MiscFlags           = D3D10_RESOURCE_MISC_SHARED;

    ID3D10Texture2D *d3d101Tex;
    if(FAILED(hErr = shareDevice->CreateTexture2D(&texGameDesc, NULL, &d3d101Tex)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoGLGPUHook: shareDevice->CreateTexture2D failed, result = " << (UINT)hErr << endl;
        goto finishGPUHook;
    }

    if(FAILED(hErr = d3d101Tex->QueryInterface(__uuidof(ID3D10Resource), (void**)&copyTextureIntermediary)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoGLGPUHook: d3d101Tex->QueryInterface(ID3D10Resource) failed, result = " << (UINT)hErr << endl;
        d3d101Tex->Release();
        goto finishGPUHook;
    }

    IDXGIResource *res;
    if(FAILED(hErr = d3d101Tex->QueryInterface(IID_IDXGIResource, (void**)&res)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoGLGPUHook: d3d101Tex->QueryInterface(IDXGIResource) failed, result = " << (UINT)hErr << endl;
        d3d101Tex->Release();
        goto finishGPUHook;
    }

    if(FAILED(res->GetSharedHandle(&sharedHandle)))
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoGLGPUHook: res->GetSharedHandle failed, result = " << (UINT)hErr << endl;
        d3d101Tex->Release();
        res->Release();
        goto finishGPUHook;
    }

    if (bNVCaptureAvailable) {
        gl_dxDevice = wglDXOpenDeviceNV(shareDevice);
        if (gl_dxDevice == NULL) {
            RUNEVERYRESET logOutput << CurrentTimeString() << "DoGLGPUHook: wglDXOpenDeviceNV failed" << endl;
            goto finishGPUHook;
        }

        glGenTextures(1, &gl_sharedtex);
        gl_handle = wglDXRegisterObjectNV(gl_dxDevice, d3d101Tex, gl_sharedtex, GL_TEXTURE_2D, WGL_ACCESS_WRITE_DISCARD_NV);

        if (gl_handle == NULL) {
            RUNEVERYRESET logOutput << CurrentTimeString() << "DoGLGPUHook: wglDXRegisterObjectNV failed" << endl;
            goto finishGPUHook;
        }
    }

    glGenFramebuffers(1, &gl_fbo);

    d3d101Tex->Release();
    res->Release();
    res = NULL;

    //------------------------------------------------

    glcaptureInfo.mapID = InitializeSharedMemoryGPUCapture(&texData);
    if(!glcaptureInfo.mapID)
    {
        RUNEVERYRESET logOutput << CurrentTimeString() << "DoGLGPUHook: failed to initialize shared memory" << endl;
        goto finishGPUHook;
    }

    bSuccess = IsWindow(hwndOBS);

finishGPUHook:

    if(bSuccess)
    {
        bHasTextures = true;
        glcaptureInfo.captureType = CAPTURETYPE_SHAREDTEX;
        glcaptureInfo.hwndCapture = (DWORD)hwndTarget;
        glcaptureInfo.bFlip = TRUE;
        texData->texHandle = (DWORD)sharedHandle;

        memcpy(infoMem, &glcaptureInfo, sizeof(CaptureInfo));
        if (!SetEvent(hSignalReady))
            logOutput << CurrentTimeString() << "SetEvent(hSignalReady) failed, GetLastError = " << UINT(GetLastError()) << endl;

        logOutput << CurrentTimeString() << "DoGLGPUHook: success" << endl;
    }
    else
        ClearGLData();
}

void DoGLCPUHook(RECT &rc)
{
    bUseSharedTextures = false;
    glcaptureInfo.cx = rc.right;
    glcaptureInfo.cy = rc.bottom;

    glGenBuffers(NUM_BUFFERS, gltextures);

    DWORD dwSize = glcaptureInfo.cx*glcaptureInfo.cy*4;

    BOOL bSuccess = true;
    for(UINT i=0; i<NUM_BUFFERS; i++)
    {
        UINT test = 0;

        glBindBuffer(GL_PIXEL_PACK_BUFFER, gltextures[i]);
        glBufferData(GL_PIXEL_PACK_BUFFER, dwSize, 0, GL_STREAM_READ);

        if(!(glDataMutexes[i] = OSCreateMutex()))
        {
            logOutput << CurrentTimeString() << "DoGLCPUHook: OSCreateMutex " << i << " failed, GetLastError = " << GetLastError();
            bSuccess = false;
            break;
        }
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    if(bSuccess)
    {
        bKillThread = false;

        if(hCopyThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CopyGLCPUTextureThread, NULL, 0, NULL))
        {
            if(!(hCopyEvent = CreateEvent(NULL, FALSE, FALSE, NULL)))
            {
                logOutput << CurrentTimeString() << "DoGLCPUHook: CreateEvent failed, GetLastError = " << GetLastError();
                bSuccess = false;
            }
        }
        else
        {
            logOutput << CurrentTimeString() << "DoGLCPUHook: CreateThread failed, GetLastError = " << GetLastError();
            bSuccess = false;
        }
    }

    if(bSuccess)
    {
        glcaptureInfo.mapID = InitializeSharedMemoryCPUCapture(dwSize, &glcaptureInfo.mapSize, &copyData, textureBuffers);
        if(!glcaptureInfo.mapID)
            bSuccess = false;
    }

    if(bSuccess)
    {
        bHasTextures = true;
        glcaptureInfo.captureType = CAPTURETYPE_MEMORY;
        glcaptureInfo.hwndCapture = (DWORD)hwndTarget;
        glcaptureInfo.pitch = glcaptureInfo.cx*4;
        glcaptureInfo.bFlip = TRUE;

        memcpy(infoMem, &glcaptureInfo, sizeof(CaptureInfo));
        SetEvent(hSignalReady);

        logOutput << CurrentTimeString() << "DoGLCPUHook: success" << endl;

        OSInitializeTimer();
    }
    else
        ClearGLData();
}

void DoGLHook(RECT &rc)
{
    if (bFBOAvailable && bNVCaptureAvailable)
        DoGLGPUHook(rc);
    else
        DoGLCPUHook(rc);
}

DWORD CopyGLCPUTextureThread(LPVOID lpUseless)
{
    int sharedMemID = 0;

    HANDLE hEvent = NULL;
    if(!DuplicateHandle(GetCurrentProcess(), hCopyEvent, GetCurrentProcess(), &hEvent, NULL, FALSE, DUPLICATE_SAME_ACCESS))
    {
        logOutput << CurrentTimeString() << "CopyGLCPUTextureThread: couldn't duplicate handle" << endl;
        return 0;
    }

    while(WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0)
    {
        if(bKillThread)
            break;

        int nextSharedMemID = sharedMemID == 0 ? 1 : 0;

        DWORD copyTex = curCPUTexture;
        LPVOID data = pCopyData;
        if(copyTex < NUM_BUFFERS && data != NULL)
        {
            OSEnterMutex(glDataMutexes[copyTex]);

            int lastRendered = -1;

            //copy to whichever is available
            if(WaitForSingleObject(textureMutexes[sharedMemID], 0) == WAIT_OBJECT_0)
                lastRendered = (int)sharedMemID;
            else if(WaitForSingleObject(textureMutexes[nextSharedMemID], 0) == WAIT_OBJECT_0)
                lastRendered = (int)nextSharedMemID;

            if(lastRendered != -1)
            {
                memcpy(textureBuffers[lastRendered], data, glcaptureInfo.pitch*glcaptureInfo.cy);
                ReleaseMutex(textureMutexes[lastRendered]);
                copyData->lastRendered = (UINT)lastRendered;
            }

            OSLeaveMutex(glDataMutexes[copyTex]);
        }

        sharedMemID = nextSharedMemID;
    }

    CloseHandle(hEvent);
    return 0;
}

bool bReacquiring = false;
LONGLONG reacquireStart = 0;
LONGLONG reacquireTime = 0;

LONG lastCX=0, lastCY=0;

void HandleGLSceneUpdate(HDC hDC)
{
    if(!bTargetAcquired && hdcAcquiredDC == NULL)
    {
        logOutput << CurrentTimeString() << "setting up gl data" << endl;
        PIXELFORMATDESCRIPTOR pfd;

        hwndTarget = WindowFromDC(hDC);

        int pixFormat = GetPixelFormat(hDC);
        DescribePixelFormat(hDC, pixFormat, sizeof(pfd), &pfd);

        if(pfd.cColorBits == 32 && hwndTarget)
        {
            bTargetAcquired = true;
            hdcAcquiredDC = hDC;
            glcaptureInfo.format = GS_BGR;
        }

        OSInitializeTimer();
    }

    if(hDC == hdcAcquiredDC)
    {
        if(bCapturing && WaitForSingleObject(hSignalEnd, 0) == WAIT_OBJECT_0)
            bStopRequested = true;

        if(bCapturing && !IsWindow(hwndOBS))
        {
            hwndOBS = NULL;
            bStopRequested = true;
        }

        if(bCapturing && bStopRequested)
        {
            RUNEVERYRESET logOutput << CurrentTimeString() << "stop requested, terminating gl capture" << endl;

            ClearGLData();
            bCapturing = false;
            bStopRequested = false;
            bReacquiring = false;
        }

        if(!bCapturing && WaitForSingleObject(hSignalRestart, 0) == WAIT_OBJECT_0)
        {
            hwndOBS = FindWindow(OBS_WINDOW_CLASS, NULL);
            if(hwndOBS)
                bCapturing = true;
        }

        RECT rc;
        GetClientRect(hwndTarget, &rc);

        if(bCapturing && bReacquiring)
        {
            if(lastCX != rc.right || lastCY != rc.bottom) //reset if continuing to size within the 3 seconds
            {
                reacquireStart = OSGetTimeMicroseconds();
                lastCX = rc.right;
                lastCY = rc.bottom;
            }

            if(OSGetTimeMicroseconds()-reacquireTime >= 3000000) { //3 second to reacquire
                RUNEVERYRESET logOutput << CurrentTimeString() << "reacquiring gl due to resize..." << endl;
                bReacquiring = false;
            } else {
                return;
            }
        }

        if(bCapturing && (!bHasTextures || rc.right != glcaptureInfo.cx || rc.bottom != glcaptureInfo.cy))
        {
            if (!rc.right || !rc.bottom)
                return;

            if(bHasTextures) //resizing
            {
                ClearGLData();
                bReacquiring = true;
                reacquireStart = OSGetTimeMicroseconds();
                lastCX = rc.right;
                lastCY = rc.bottom;
                return;
            }
            else
            {
                if(hwndOBS)
                    DoGLHook(rc);
                else
                    ClearGLData();
            }
        }

        LONGLONG timeVal = OSGetTimeMicroseconds();

        //check keep alive state, dumb but effective
        if(bCapturing)
        {
            if (!keepAliveTime)
                keepAliveTime = timeVal;

            if((timeVal-keepAliveTime) > 5000000)
            {
                HANDLE hKeepAlive = OpenEvent(EVENT_ALL_ACCESS, FALSE, strKeepAlive.c_str());
                if (hKeepAlive) {
                    CloseHandle(hKeepAlive);
                } else {
                    ClearGLData();
                    logOutput << CurrentTimeString() << "Keepalive no longer found on gl, freeing capture data" << endl;
                    bCapturing = false;
                }

                keepAliveTime = timeVal;
            }
        }

        if(bHasTextures)
        {
            LONGLONG frameTime;
            if(bCapturing)
            {
                if (bUseSharedTextures) {
                    if (texData) {
                        if(frameTime = texData->frameTime) {
                            LONGLONG timeElapsed = timeVal-lastTime;

                            if(timeElapsed >= frameTime) {
                                lastTime += frameTime;
                                if(timeElapsed > frameTime*2)
                                    lastTime = timeVal;

                                wglDXLockObjectsNV(gl_dxDevice, 1, &gl_handle);

                                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gl_fbo);

                                glBindTexture(GL_TEXTURE_2D, gl_sharedtex);
                                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_sharedtex, 0);

                                glReadBuffer(GL_BACK); //source
                                glDrawBuffer(GL_COLOR_ATTACHMENT0); //dest

                                glBlitFramebuffer(0, 0, glcaptureInfo.cx, glcaptureInfo.cy, 0, 0, glcaptureInfo.cx, glcaptureInfo.cy, GL_COLOR_BUFFER_BIT, GL_NEAREST);

                                glBindTexture(GL_TEXTURE_2D, 0);
                                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

                                wglDXUnlockObjectsNV(gl_dxDevice, 1, &gl_handle);
                            }
                        }
                    }
                } else {
                    if(copyData)
                    {
                        if(frameTime = copyData->frameTime)
                        {
                            LONGLONG timeElapsed = timeVal-lastTime;

                            if(timeElapsed >= frameTime)
                            {
                                lastTime += frameTime;
                                if(timeElapsed > frameTime*2)
                                    lastTime = timeVal;

                                GLuint texture = gltextures[curCapture];
                                DWORD nextCapture = (curCapture == NUM_BUFFERS-1) ? 0 : (curCapture+1);

                                glReadBuffer(GL_BACK);
                                glBindBuffer(GL_PIXEL_PACK_BUFFER, texture);

                                if(glLockedTextures[curCapture])
                                {
                                    OSEnterMutex(glDataMutexes[curCapture]);

                                    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
                                    glLockedTextures[curCapture] = false;

                                    OSLeaveMutex(glDataMutexes[curCapture]);
                                }

                                glReadPixels(0, 0, glcaptureInfo.cx, glcaptureInfo.cy, GL_BGRA, GL_UNSIGNED_BYTE, 0);

                                //----------------------------------

                                glBindBuffer(GL_PIXEL_PACK_BUFFER, gltextures[nextCapture]);
                                pCopyData = (void*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
                                if(pCopyData)
                                {
                                    curCPUTexture = nextCapture;
                                    glLockedTextures[nextCapture] = true;

                                    RUNEVERYRESET logOutput << CurrentTimeString() << "successfully capturing gl frames via RAM" << endl;

                                    SetEvent(hCopyEvent);
                                } else {
                                    RUNEVERYRESET logOutput << CurrentTimeString() << "one or more gl frames failed to capture for some reason" << endl;
                                }

                                //----------------------------------

                                glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

                                curCapture = nextCapture;
                            }
                        }
                    }
                }
            }
            else {
                RUNEVERYRESET logOutput << CurrentTimeString() << "no longer capturing, terminating gl capture" << endl;
                ClearGLData();
            }
        }
    }
}

static void HandleGLSceneDestroy()
{
    ClearGLData();
    hdcAcquiredDC = NULL;
    bTargetAcquired = false;
}


static BOOL WINAPI SwapBuffersHook(HDC hDC)
{
    HandleGLSceneUpdate(hDC);

    glHookSwapBuffers.Unhook();
    BOOL bResult = SwapBuffers(hDC);
    glHookSwapBuffers.Rehook(); 

    return bResult;
}

static BOOL WINAPI wglSwapLayerBuffersHook(HDC hDC, UINT fuPlanes)
{
    HandleGLSceneUpdate(hDC);

    glHookSwapLayerBuffers.Unhook();
    BOOL bResult = jimglSwapLayerBuffers(hDC, fuPlanes);
    glHookSwapLayerBuffers.Rehook();

    return bResult;
}

static BOOL WINAPI wglSwapBuffersHook(HDC hDC)
{
    HandleGLSceneUpdate(hDC);

    glHookwglSwapBuffers.Unhook();
    BOOL bResult = jimglSwapBuffers(hDC);
    glHookwglSwapBuffers.Rehook();

    return bResult;
}

static BOOL WINAPI wglDeleteContextHook(HGLRC hRC)
{
    HandleGLSceneDestroy();

    glHookDeleteContext.Unhook();
    BOOL bResult = jimglDeleteContext(hRC);
    glHookDeleteContext.Rehook();

    return bResult;
}

static void RegisterNVCaptureStuff()
{
    wglDXSetResourceShareHandleNV = (WGLSETRESOURCESHAREHANDLENVPROC)jimglGetProcAddress("wglDXSetResourceShareHandleNV");
    wglDXOpenDeviceNV             = (WGLDXOPENDEVICENVPROC)          jimglGetProcAddress("wglDXOpenDeviceNV");
    wglDXCloseDeviceNV            = (WGLDXCLOSEDEVICENVPROC)         jimglGetProcAddress("wglDXCloseDeviceNV");
    wglDXRegisterObjectNV         = (WGLDXREGISTEROBJECTNVPROC)      jimglGetProcAddress("wglDXRegisterObjectNV");
    wglDXUnregisterObjectNV       = (WGLDXUNREGISTEROBJECTNVPROC)    jimglGetProcAddress("wglDXUnregisterObjectNV");
    wglDXObjectAccessNV           = (WGLDXOBJECTACCESSNVPROC)        jimglGetProcAddress("wglDXObjectAccessNV");
    wglDXLockObjectsNV            = (WGLDXLOCKOBJECTSNVPROC)         jimglGetProcAddress("wglDXLockObjectsNV");
    wglDXUnlockObjectsNV          = (WGLDXUNLOCKOBJECTSNVPROC)       jimglGetProcAddress("wglDXUnlockObjectsNV");

    bNVCaptureAvailable = wglDXSetResourceShareHandleNV && wglDXOpenDeviceNV && wglDXCloseDeviceNV &&
                          wglDXRegisterObjectNV && wglDXUnregisterObjectNV && wglDXObjectAccessNV &&
                          wglDXLockObjectsNV && wglDXUnlockObjectsNV;

    if (bNVCaptureAvailable)
        logOutput << CurrentTimeString() << "NV Capture available" << endl;
}

static void RegisterFBOStuff()
{
    glGenFramebuffers      = (GLGENFRAMEBUFFERSPROC)     jimglGetProcAddress("glGenFramebuffers");
    glDeleteFramebuffers   = (GLDELETEFRAMEBUFFERSPROC)  jimglGetProcAddress("glDeleteFramebuffers");
    glBindFramebuffer      = (GLBINDFRAMEBUFFERPROC)     jimglGetProcAddress("glBindFramebuffer");
    glBlitFramebuffer      = (GLBLITFRAMEBUFFERPROC)     jimglGetProcAddress("glBlitFramebuffer");
    glFramebufferTexture2D = (GLFRAMEBUFFERTEXTURE2DPROC)jimglGetProcAddress("glFramebufferTexture2D");

    bFBOAvailable = glGenFramebuffers && glDeleteFramebuffers && glBindFramebuffer &&
                    glBlitFramebuffer && glFramebufferTexture2D;

    if (bNVCaptureAvailable)
        logOutput << CurrentTimeString() << "FBO available" << endl;
}

bool InitGLCapture()
{
    static HWND hwndOpenGLSetupWindow = NULL;
    bool bSuccess = false;

    if(!hwndOpenGLSetupWindow)
    {
        WNDCLASSEX windowClass;

        ZeroMemory(&windowClass, sizeof(windowClass));

        windowClass.cbSize = sizeof(windowClass);
        windowClass.style = CS_OWNDC;
        windowClass.lpfnWndProc = DefWindowProc;
        windowClass.lpszClassName = TEXT("OBSOGLHookClass");
        windowClass.hInstance = hinstMain;

        if(RegisterClassEx(&windowClass))
        {
            hwndOpenGLSetupWindow = CreateWindowEx (0,
                TEXT("OBSOGLHookClass"),
                TEXT("OBS OpenGL Context Window"),
                WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                0, 0,
                1, 1,
                NULL,
                NULL,
                hinstMain,
                NULL
                );
        }
    }

    HMODULE hGL = GetModuleHandle(TEXT("opengl32.dll"));
    if(hGL && hwndOpenGLSetupWindow)
    {
        glReadBuffer          = (GLREADBUFFERPROC)       GetProcAddress(hGL, "glReadBuffer");
        glDrawBuffer          = (GLDRAWBUFFERPROC)       GetProcAddress(hGL, "glDrawBuffer");
        glReadPixels          = (GLREADPIXELSPROC)       GetProcAddress(hGL, "glReadPixels");
        glGetError            = (GLGETERRORPROC)         GetProcAddress(hGL, "glGetError");
        jimglSwapLayerBuffers = (WGLSWAPLAYERBUFFERSPROC)GetProcAddress(hGL, "wglSwapLayerBuffers");
        jimglSwapBuffers      = (WGLSWAPBUFFERSPROC)     GetProcAddress(hGL, "wglSwapBuffers");
        jimglDeleteContext    = (WGLDELETECONTEXTPROC)   GetProcAddress(hGL, "wglDeleteContext");
        jimglGetProcAddress   = (WGLGETPROCADDRESSPROC)  GetProcAddress(hGL, "wglGetProcAddress");
        jimglMakeCurrent      = (WGLMAKECURRENTPROC)     GetProcAddress(hGL, "wglMakeCurrent");
        jimglCreateContext    = (WGLCREATECONTEXTPROC)   GetProcAddress(hGL, "wglCreateContext");

        if( !glReadBuffer || !glReadPixels || !glGetError || !jimglSwapLayerBuffers || !jimglSwapBuffers ||
            !jimglDeleteContext || !jimglGetProcAddress || !jimglMakeCurrent || !jimglCreateContext)
        {
            return false;
        }

        HDC hDC = GetDC(hwndOpenGLSetupWindow);
        if(hDC)
        {
            PIXELFORMATDESCRIPTOR pfd;
            ZeroMemory(&pfd, sizeof(pfd));
            pfd.nSize = sizeof(pfd);
            pfd.nVersion = 1;
            pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_GENERIC_ACCELERATED;
            pfd.iPixelType = PFD_TYPE_RGBA;
            pfd.cColorBits = 32;
            pfd.cDepthBits = 32;
            pfd.cAccumBits = 32;
            pfd.iLayerType = PFD_MAIN_PLANE;
            SetPixelFormat(hDC, ChoosePixelFormat(hDC, &pfd), &pfd);

            HGLRC hGlrc = jimglCreateContext(hDC);
            if(hGlrc)
            {
                jimglMakeCurrent(hDC, hGlrc);

                glBufferData       = (GLBUFFERDATAARBPROC)     jimglGetProcAddress("glBufferData");
                glDeleteBuffers    = (GLDELETEBUFFERSARBPROC)  jimglGetProcAddress("glDeleteBuffers");
                glDeleteTextures   = (GLDELETETEXTURESPROC)    GetProcAddress(hGL, "glDeleteTextures");
                glGenBuffers       = (GLGENBUFFERSARBPROC)     jimglGetProcAddress("glGenBuffers");
                glGenTextures      = (GLGENTEXTURESPROC)       GetProcAddress(hGL, "glGenTextures");
                glMapBuffer        = (GLMAPBUFFERPROC)         jimglGetProcAddress("glMapBuffer");
                glUnmapBuffer      = (GLUNMAPBUFFERPROC)       jimglGetProcAddress("glUnmapBuffer");
                glBindBuffer       = (GLBINDBUFFERPROC)        jimglGetProcAddress("glBindBuffer");
                glBindTexture      = (GLBINDTEXTUREPROC)       GetProcAddress(hGL, "glBindTexture");

                UINT lastErr = GetLastError();

                RegisterNVCaptureStuff();
                RegisterFBOStuff();

                if(glBufferData && glDeleteBuffers && glDeleteTextures && glGenBuffers &&
                    glGenTextures && glMapBuffer && glUnmapBuffer && glBindBuffer && glBindTexture)
                {
                    glHookSwapBuffers.Hook((FARPROC)SwapBuffers, (FARPROC)SwapBuffersHook);
                    glHookSwapLayerBuffers.Hook((FARPROC)jimglSwapLayerBuffers, (FARPROC)wglSwapLayerBuffersHook);
                    glHookwglSwapBuffers.Hook((FARPROC)jimglSwapBuffers, (FARPROC)wglSwapBuffersHook);
                    glHookDeleteContext.Hook((FARPROC)jimglDeleteContext, (FARPROC)wglDeleteContextHook);
                    bSuccess = true;
                }

                jimglMakeCurrent(NULL, NULL);

                jimglDeleteContext(hGlrc);

                ReleaseDC(hwndOpenGLSetupWindow, hDC);

                if(bSuccess)
                {
                    glHookSwapBuffers.Rehook();
                    glHookSwapLayerBuffers.Rehook();
                    glHookwglSwapBuffers.Rehook();
                    glHookDeleteContext.Rehook();

                    DestroyWindow(hwndOpenGLSetupWindow);
                    hwndOpenGLSetupWindow = NULL;

                    UnregisterClass(TEXT("OBSOGLHookClass"), hinstMain);
                }
            }

            if(hwndOpenGLSetupWindow)
                ReleaseDC(hwndOpenGLSetupWindow, hDC);
        }
    }

    return bSuccess;
}

void FreeGLCapture()
{
    glHookSwapBuffers.Unhook();
    glHookSwapLayerBuffers.Unhook();
    glHookwglSwapBuffers.Unhook();
    glHookDeleteContext.Unhook();

    ClearGLData();
}
