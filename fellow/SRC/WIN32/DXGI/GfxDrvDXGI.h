#ifndef GfxDrvDXGI_H
#define GfxDrvDXGI_H

#include <DXGI.h>
#include <d3d11.h>

#include "DEFS.H"
#include "GfxDrvDXGIAdapter.h"
#include "DRAW.H"

const static unsigned int AmigaScreenTextureCount = 1;
const static unsigned int BackBufferCount = 2;

class GfxDrvDXGI
{
private:
  // Information
  GfxDrvDXGIAdapterList* _adapters;
  IDXGIFactory *_enumerationFactory;

  // Current session
  ID3D11Device *_d3d11device;
  ID3D11DeviceContext *_immediateContext;
  IDXGIFactory *_dxgiFactory;
  IDXGISwapChain *_swapChain;
  unsigned int _amigaScreenTextureCount;
  unsigned int _currentAmigaScreenTexture;
  ID3D11Texture2D *_amigaScreenTexture[AmigaScreenTextureCount];
  draw_mode *_current_draw_mode;

private:
  void CreateAdapterList();
  void DeleteAdapterList();

  bool CreateEnumerationFactory();
  void DeleteEnumerationFactory();

  void RegisterMode(unsigned int id, unsigned int width, unsigned int height, unsigned int refreshRate = 60, bool isWindowed = true);
  void RegisterModes();
  void AddFullScreenModes();
  void AddWindowModes();

  bool CreateD3D11Device();
  void DeleteD3D11Device();
  
  void DeleteDXGIFactory();
  void DeleteImmediateContext();

  bool CreateSwapChain();
  void DeleteSwapChain();
  bool InitiateSwitchToFullScreen();
  DXGI_MODE_DESC* GetDXGIMode(unsigned int id);

  bool CreateAmigaScreenTexture();
  void DeleteAmigaScreenTexture();
  ID3D11Texture2D *GetCurrentAmigaScreenTexture();

  void FlipTexture();

  STR* GetFeatureLevelString(D3D_FEATURE_LEVEL featureLevel);

public:

  void ClearCurrentBuffer();
  void SetMode(draw_mode *dm);
  void SizeChanged();
  void NotifyActiveStatus(bool active);

  bool EmulationStart(unsigned int maxbuffercount);
  unsigned int EmulationStartPost();
  void EmulationStop();

  bool Startup();
  void Shutdown();

  unsigned char *ValidateBufferPointer();
  void InvalidateBufferPointer();
  void Flip();

  void RegisterRetroPlatformScreenMode(const bool, const ULO, const ULO, const ULO);
  bool SaveScreenshot(const bool, const STR *);

  GfxDrvDXGI();
  virtual ~GfxDrvDXGI();
};

#endif
