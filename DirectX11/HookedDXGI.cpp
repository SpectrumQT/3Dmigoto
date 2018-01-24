// Object			OS				DXGI version	Feature level
// IDXGIFactory		Win7			1.0				11.0
// IDXGIFactory1	Win7			1.0				11.0
// IDXGIFactory2	Platform update	1.2				11.1
// IDXGIFactory3	Win8.1			1.3
// IDXGIFactory4					1.4
// IDXGIFactory5					1.5
//
// IDXGISwapChain	Win7			1.0				11.0
// IDXGISwapChain1	Platform update	1.2				11.1
// IDXGISwapChain2	Win8.1			1.3
// IDXGISwapChain3	Win10			1.4
// IDXGISwapChain4					1.5

#include <d3d11_1.h>

#include "HookedDXGI.h"
#include "HackerDXGI.h"

#include "DLLMainHook.h"
#include "log.h"
#include "util.h"
#include "D3D11Wrapper.h"


// This class is for a different approach than the wrapping of the system objects
// like we do with ID3D11Device for example.  When we wrap a COM object like that,
// it's not a real C++ object, and consequently cannot use the superclass normally,
// and requires boilerplate call-throughs for every interface to the object.  We
// may only care about a 5 calls, but we have to wrap all 150 calls. 
//
// Rather than do that with DXGI, this approach will be to singly hook the calls we
// are interested in, using the Nektra In-Proc hooking.  We'll still create
// objects for encapsulation where necessary, by returning HackerDXGIFactory1
// and HackerDXGIFactory2 when platform_update is set.  We won't ever return
// HackerDXGIFactory because the minimum on Win7 is IDXGIFactory1.
//
// For our hooks:
// It is worth noting, since it took me 3 days to figure it out, than even though
// they are defined C style, that we must use STDMETHODCALLTYPE (or__stdcall) 
// because otherwise the stack is broken by the different calling conventions.
//
// In normal method calls, the 'this' parameter is implicitly added.  Since we are
// using the C style dxgi interface though, we are declaring these routines differently.
//
// Since we want to allow reentrancy for the calls, we need to use the returned
// fnOrig* to call the original, instead of the alternate approach offered by
// Deviare.




// -----------------------------------------------------------------------------
// This tweaks the parameters passed to the real CreateSwapChain, to change behavior.
// These global parameters come originally from the d3dx.ini, so the user can
// change them.
//
// There is now also ForceDisplayParams1 which has some overlap.

void ForceDisplayParams(DXGI_SWAP_CHAIN_DESC *pDesc)
{
	if (pDesc == NULL)
		return;

	LogInfo("     Windowed = %d\n", pDesc->Windowed);
	LogInfo("     Width = %d\n", pDesc->BufferDesc.Width);
	LogInfo("     Height = %d\n", pDesc->BufferDesc.Height);
	LogInfo("     Refresh rate = %f\n",
		(float)pDesc->BufferDesc.RefreshRate.Numerator / (float)pDesc->BufferDesc.RefreshRate.Denominator);

	if (G->SCREEN_UPSCALING == 0 && G->SCREEN_FULLSCREEN > 0)
	{
		pDesc->Windowed = false;
		LogInfo("->Forcing Windowed to = %d\n", pDesc->Windowed);
	}

	if (G->SCREEN_FULLSCREEN == 2 || G->SCREEN_UPSCALING > 0)
	{
		// We install this hook on demand to avoid any possible
		// issues with hooking the call when we don't need it:
		// Unconfirmed, but possibly related to:
		// https://forums.geforce.com/default/topic/685657/3d-vision/3dmigoto-now-open-source-/post/4801159/#4801159
		//
		// This hook is also very important in case of Upscaling
		InstallSetWindowPosHook();
	}

	if (G->SCREEN_REFRESH >= 0 && !pDesc->Windowed)
	{
		pDesc->BufferDesc.RefreshRate.Numerator = G->SCREEN_REFRESH;
		pDesc->BufferDesc.RefreshRate.Denominator = 1;
		LogInfo("->Forcing refresh rate to = %f\n",
			(float)pDesc->BufferDesc.RefreshRate.Numerator / (float)pDesc->BufferDesc.RefreshRate.Denominator);
	}
	if (G->SCREEN_WIDTH >= 0)
	{
		pDesc->BufferDesc.Width = G->SCREEN_WIDTH;
		LogInfo("->Forcing Width to = %d\n", pDesc->BufferDesc.Width);
	}
	if (G->SCREEN_HEIGHT >= 0)
	{
		pDesc->BufferDesc.Height = G->SCREEN_HEIGHT;
		LogInfo("->Forcing Height to = %d\n", pDesc->BufferDesc.Height);
	}

	// To support 3D Vision Direct Mode, we need to force the backbuffer from the
	// swapchain to be 2x its normal width.  
	if (G->gForceStereo == 2)
	{
		pDesc->BufferDesc.Width *= 2;
		LogInfo("->Direct Mode: Forcing Width to = %d\n", pDesc->BufferDesc.Width);
	}
}

// Different variant for the CreateSwapChainForHwnd.
//
// We absolutely need the force full screen in order to enable 3D.  
// Batman Telltale needs this.
// The rest of the variants are less clear.

void ForceDisplayParams1(DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc1)
{
	if (pDesc1 == NULL)
		return;

	LogInfo("     Windowed = %d\n", pDesc1->Windowed);

	if (G->SCREEN_FULLSCREEN > 0)
	{
		pDesc1->Windowed = false;
		LogInfo("->Forcing Windowed to = %d\n", pDesc1->Windowed);
	}

	if (G->SCREEN_FULLSCREEN == 2)
	{
		// We install this hook on demand to avoid any possible
		// issues with hooking the call when we don't need it:
		// Unconfirmed, but possibly related to:
		// https://forums.geforce.com/default/topic/685657/3d-vision/3dmigoto-now-open-source-/post/4801159/#4801159

		InstallSetWindowPosHook();
	}

	// These input parameters are not clear how to implement for CreateSwapChainForHwnd,
	// and are stubbed out with error reporting. Can be implemented when cases arise.
	if (G->SCREEN_REFRESH >= 0 && !pDesc1->Windowed)
	{
		LogInfo("*** Unimplemented feature for refresh_rate in CreateSwapChainForHwnd\n");
		BeepFailure();
	}
	if (G->SCREEN_WIDTH >= 0)
	{
		LogInfo("*** Unimplemented feature to force screen width in CreateSwapChainForHwnd\n");
		BeepFailure();
	}
	if (G->SCREEN_HEIGHT >= 0)
	{
		LogInfo("*** Unimplemented feature to force screen height in CreateSwapChainForHwnd\n");
		BeepFailure();
	}

	// To support 3D Vision Direct Mode, we need to force the backbuffer from the
	// swapchain to be 2x its normal width.  
	if (G->gForceStereo == 2)
	{
		LogInfo("*** Unimplemented feature for Direct Mode in CreateSwapChainForHwnd\n");
		BeepFailure();
	}
}


// -----------------------------------------------------------------------------
// Actual hook for any IDXGICreateSwapChainForHwnd calls the game makes.
// This can only be called with Win7+platform_update or greater, using
// the IDXGIFactory2.
// 
// This type of SwapChain cannot be made through the CreateDeviceAndSwapChain,
// so there is only one logical path to create this, which is 
// IDXGIFactory2->CreateSwapChainForHwnd.  That means that the Device has
// already been created with CreateDevice, and dereferenced through the 
// chain of QueryInterface calls to get the IDXGIFactory2.

HRESULT(__stdcall *fnOrigCreateSwapChainForHwnd)(
	IDXGIFactory2 * This,
	/* [annotation][in] */
	_In_  IUnknown *pDevice,
	/* [annotation][in] */
	_In_  HWND hWnd,
	/* [annotation][in] */
	_In_  const DXGI_SWAP_CHAIN_DESC1 *pDesc,
	/* [annotation][in] */
	_In_opt_  const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
	/* [annotation][in] */
	_In_opt_  IDXGIOutput *pRestrictToOutput,
	/* [annotation][out] */
	_Out_  IDXGISwapChain1 **ppSwapChain) = nullptr;


HRESULT __stdcall Hooked_CreateSwapChainForHwnd(
	IDXGIFactory2 * This,
	/* [annotation][in] */
	_In_  IUnknown *pDevice,
	/* [annotation][in] */
	_In_  HWND hWnd,
	/* [annotation][in] */
	_In_  const DXGI_SWAP_CHAIN_DESC1 *pDesc,
	/* [annotation][in] */
	_In_opt_  const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
	/* [annotation][in] */
	_In_opt_  IDXGIOutput *pRestrictToOutput,
	/* [annotation][out] */
	_Out_  IDXGISwapChain1 **ppSwapChain)
{
	LogInfo("Hooked IDXGIFactory2::CreateSwapChainForHwnd(%p) called\n", This);
	LogInfo("  Device = %p\n", pDevice);
	LogInfo("  SwapChain = %p\n", ppSwapChain);
	LogInfo("  Description1 = %p\n", pDesc);
	LogInfo("  FullScreenDescription = %p\n", pFullscreenDesc);

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullScreenDesc = { 0 };
	if (!pFullscreenDesc)
		pFullscreenDesc = &fullScreenDesc;
	ForceDisplayParams1(&fullScreenDesc);

	HRESULT hr = fnOrigCreateSwapChainForHwnd(This, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
	if (FAILED(hr))
	{
		LogInfo("->Failed result %#x\n\n", hr);
		return hr;
	}

	IDXGISwapChain1* origSwapChain = *ppSwapChain;
	HackerDevice* hackerDevice = reinterpret_cast<HackerDevice*>(pDevice);
	HackerContext* hackerContext = hackerDevice->GetHackerContext();

	HackerSwapChain* hackerSwapChain;
	hackerSwapChain = new HackerSwapChain(origSwapChain, hackerDevice, hackerContext);


	// When creating a new swapchain, we can assume this is the game creating 
	// the most important object, and return the wrapped swapchain to the game 
	// so it will call our Present.
	*ppSwapChain = hackerSwapChain;

	LogInfo("->return result %#x, HackerSwapChain = %p wrapper of ppSwapChain = %p\n\n", hr, hackerSwapChain, origSwapChain);
	return hr;
}

// -----------------------------------------------------------------------------
// This hook should work in all variants, including the CreateSwapChain1
// and CreateSwapChainForHwnd

void HookCreateSwapChainForHwnd(void* factory2)
{
	LogInfo("*** IDXGIFactory2 creating hook for CreateSwapChainForHwnd. \n");

	IDXGIFactory2* dxgiFactory = reinterpret_cast<IDXGIFactory2*>(factory2);

	SIZE_T hook_id;
	DWORD dwOsErr = cHookMgr.Hook(&hook_id, (void**)&fnOrigCreateSwapChainForHwnd,
		lpvtbl_CreateSwapChainForHwnd(dxgiFactory), Hooked_CreateSwapChainForHwnd, 0);

	if (dwOsErr == ERROR_SUCCESS)
		LogInfo("  Successfully installed IDXGIFactory2->CreateSwapChainForHwnd hook.\n");
	else
		LogInfo("  *** Failed install IDXGIFactory2->CreateSwapChainForHwnd hook.\n");
}

// -----------------------------------------------------------------------------
// Actual hook for any IDXGICreateSwapChain calls the game makes.
//
// There are two primary paths that can arrive here.
// ---1. d3d11->CreateDeviceAndSwapChain
//	This path arrives here with a normal ID3D11Device1 device, not a HackerDevice.
//	This is called implictly from the middle of CreateDeviceAndSwapChain.---
//	No longer necessary, with CreateDeviceAndSwapChain broken into two direct calls.
// 2. IDXGIFactory->CreateSwapChain after CreateDevice
//	This path requires a pDevice passed in, which is a HackerDevice.  This is the
//	secret path, where they take the Device and QueryInterface to get IDXGIDevice
//	up to getting Factory, where they call CreateSwapChain. In this path, we can
//	expect the input pDevice to have already been setup as a HackerDevice.
//
//
// In prior code, we were looking for possible IDXGIDevice's as the pDevice input.
// That should not be a problem now, because we are specifically trying to cast
// that input into an ID3D11Device1 using QueryInterface.  Leaving the original
// code commented out at the bottom of the file, for reference.

HRESULT(__stdcall *fnOrigCreateSwapChain)(
	IDXGIFactory * This,
	/* [annotation][in] */
	_In_  IUnknown *pDevice,
	/* [annotation][in] */
	_In_  DXGI_SWAP_CHAIN_DESC *pDesc,
	/* [annotation][out] */
	_Out_  IDXGISwapChain **ppSwapChain) = nullptr;


HRESULT __stdcall Hooked_CreateSwapChain(
	IDXGIFactory * This,
	/* [annotation][in] */
	_In_  IUnknown *pDevice,
	/* [annotation][in] */
	_In_  DXGI_SWAP_CHAIN_DESC *pDesc,
	/* [annotation][out] */
	_Out_  IDXGISwapChain **ppSwapChain)
{
	LogInfo("\nHooked IDXGIFactory::CreateSwapChain(%p) called\n", This);
	LogInfo("  Device = %p\n", pDevice);
	LogInfo("  SwapChain = %p\n", ppSwapChain);
	LogInfo("  Description = %p\n", pDesc);

	// If hooking is enabled, pDevice will be the original DirectX object
	// with hooks to call into our code. Try looking up the corresponding
	// HackerDevice and use it if found:
	HackerDevice *hackerDevice = (HackerDevice*)lookup_hooked_device((ID3D11Device1*)pDevice);
	if (!hackerDevice)
	{
		// Without hooking pDevice input is always going to be a
		// HackerDevice, because the startup path now builds
		// HackerDevice before creating a swapchain.
		hackerDevice = reinterpret_cast<HackerDevice*>(pDevice);
	}
	HackerContext* hackerContext = hackerDevice->GetHackerContext();


	DXGI_SWAP_CHAIN_DESC origSwapChainDesc;
	if (pDesc != nullptr)
	{
		// Save window handle so we can translate mouse coordinates to the window:
		G->hWnd = pDesc->OutputWindow;

		if (G->SCREEN_UPSCALING > 0)
		{
			// Copy input swap chain desc in case it's modified
			memcpy(&origSwapChainDesc, pDesc, sizeof(DXGI_SWAP_CHAIN_DESC));

			// For the upscaling case, fullscreen has to be set after swap chain is created
			pDesc->Windowed = true;
		}

		// Required in case the software mouse and upscaling are on at the same time
		// TODO: Use a helper class to track *all* different resolutions
		G->GAME_INTERNAL_WIDTH = pDesc->BufferDesc.Width;
		G->GAME_INTERNAL_HEIGHT = pDesc->BufferDesc.Height;

		if (G->mResolutionInfo.from == GetResolutionFrom::SWAP_CHAIN)
		{
			// TODO: Use a helper class to track *all* different resolutions
			G->mResolutionInfo.width = pDesc->BufferDesc.Width;
			G->mResolutionInfo.height = pDesc->BufferDesc.Height;
			LogInfo("Got resolution from swap chain: %ix%i\n",
				G->mResolutionInfo.width, G->mResolutionInfo.height);
		}
	}

	ForceDisplayParams(pDesc);

	HRESULT hr = fnOrigCreateSwapChain(This, pDevice, pDesc, ppSwapChain);
	if (FAILED(hr))
	{
		LogInfo("->Failed result %#x\n\n", hr);
		return hr;
	}

	// Always upcast to IDXGISwapChain1 whenever possible.
	// If the upcast fails, that means we have a normal IDXGISwapChain,
	// but we'll still store it as an IDXGISwapChain1.  It's a little
	// weird to reinterpret this way, but should cause no problems in
	// the Win7 no platform_udpate case.
	IDXGISwapChain1* origSwapChain;
	(*ppSwapChain)->QueryInterface(IID_PPV_ARGS(&origSwapChain));
	if (origSwapChain == nullptr)
		origSwapChain = reinterpret_cast<IDXGISwapChain1*>(*ppSwapChain);


	// Original swapchain has been successfully created. Now we want to 
	// wrap the returned swapchain as either HackerSwapChain or HackerUpscalingSwapChain.  
	HackerSwapChain* swapchainWrap;

	if (G->SCREEN_UPSCALING == 0)		// Normal case
	{
		swapchainWrap = new HackerSwapChain(origSwapChain, hackerDevice, hackerContext);
		LogInfo("->HackerSwapChain %p created to wrap %p\n", swapchainWrap, *ppSwapChain);
	}
	else								// Upscaling case
	{
		swapchainWrap = new HackerUpscalingSwapChain(origSwapChain, hackerDevice, hackerContext,
			&origSwapChainDesc, pDesc->BufferDesc.Width, pDesc->BufferDesc.Height, This);
		LogInfo("  HackerUpscalingSwapChain %p created to wrap %p.\n", swapchainWrap, *ppSwapChain);

		if (G->SCREEN_UPSCALING == 2 || !origSwapChainDesc.Windowed)
		{
			// Some games react very strange (like render nothing) if set full screen state is called here)
			// Other games like The Witcher 3 need the call to ensure entering the full screen on start
			// (seems to be game internal stuff)  ToDo: retest if this is still necessary, lots of changes.
			(*ppSwapChain)->SetFullscreenState(TRUE, nullptr);
		}
	}

	// When creating a new swapchain, we can assume this is the game creating 
	// the most important object. Return the wrapped swapchain to the game so it 
	// will call our Present.
	*ppSwapChain = swapchainWrap;

	LogInfo("->return result %#x, HackerSwapChain = %p wrapper of ppSwapChain = %p\n\n", hr, swapchainWrap, origSwapChain);
	return hr;
}

// -----------------------------------------------------------------------------
// This hook should work in all variants, including the CreateSwapChain1
// and CreateSwapChainForHwnd

void HookCreateSwapChain(void* factory)
{
	LogInfo("*** IDXGIFactory creating hook for CreateSwapChain. \n");

	IDXGIFactory* dxgiFactory = reinterpret_cast<IDXGIFactory*>(factory);

	SIZE_T hook_id;
	DWORD dwOsErr = cHookMgr.Hook(&hook_id, (void**)&fnOrigCreateSwapChain,
		lpvtbl_CreateSwapChain(dxgiFactory), Hooked_CreateSwapChain, 0);

	if (dwOsErr == ERROR_SUCCESS)
		LogInfo("  Successfully installed IDXGIFactory->CreateSwapChain hook.\n");
	else
		LogInfo("  *** Failed install IDXGIFactory->CreateSwapChain hook.\n");
}


// -----------------------------------------------------------------------------
// Actual function called by the game for every CreateDXGIFactory they make.
// This is only called for the in-process game, not system wide.
//
// We are going to always upcast to an IDXGIFactory2 for any calls here.
// The only time we'll not use Factory2 is on Win7 without the evil update.

HRESULT(__stdcall *fnOrigCreateDXGIFactory)(
	REFIID riid,
	_Out_ void   **ppFactory
	) = nullptr;

HRESULT __stdcall Hooked_CreateDXGIFactory(REFIID riid, void **ppFactory)
{
	LogInfo("*** Hooked_CreateDXGIFactory called with riid: %s\n", NameFromIID(riid).c_str());

	// If this happens to be first call from the game, let's make sure to load
	// up our d3d11.dll and the .ini file.
	InitD311();

	// If we are being requested to create a DXGIFactory2, lie and say it's not possible.
	if (riid == __uuidof(IDXGIFactory2) && !G->enable_platform_update)
	{
		LogInfo("  returns E_NOINTERFACE as error for IDXGIFactory2.\n");
		*ppFactory = NULL;
		return E_NOINTERFACE;
	}

	HRESULT hr = fnOrigCreateDXGIFactory(riid, ppFactory);
	if (FAILED(hr))
	{
		LogInfo("->failed with HRESULT=%x\n", hr);
		return hr;
	}

	if (!fnOrigCreateSwapChain)
		HookCreateSwapChain(*ppFactory);

	// With the addition of the platform_update, we need to allow for specifically
	// creating a DXGIFactory2 instead of DXGIFactory1.  We want to always upcast
	// the highest supported object for each scenario, to properly suppport
	// QueryInterface and GetParent upcasts.

	IDXGIFactory2* dxgiFactory = reinterpret_cast<IDXGIFactory2*>(*ppFactory);
	HRESULT res = dxgiFactory->QueryInterface(IID_PPV_ARGS(&dxgiFactory));
	if (SUCCEEDED(res))
	{
		*ppFactory = (void*)dxgiFactory;
		LogInfo("  Upcast QueryInterface(IDXGIFactory2) returned result = %x, factory = %p\n", res, dxgiFactory);

		if (!fnOrigCreateSwapChainForHwnd)
			HookCreateSwapChainForHwnd(*ppFactory);
	}

	LogInfo("  CreateDXGIFactory returned factory = %p, result = %x\n", *ppFactory, hr);
	return hr;
}


// -----------------------------------------------------------------------------
//
// We are going to always upcast to an IDXGIFactory2 for any calls here.
// The only time we'll not use Factory2 is on Win7 without the evil update.
//
// ToDo: It is probably possible for a game to fetch a Factory2 via QueryInterface,
//  and we might need to hook that as well.  However, in order to Query, they
//  need a Factory or Factory1 to do so, which will call us here anyway.  At least
//  until Win10, where the d3d11.dll also then includes CreateDXGIFactory2. We only 
//  really care about installing a hook for CreateSwapChain which will still get done.

HRESULT(__stdcall *fnOrigCreateDXGIFactory1)(
	REFIID riid,
	_Out_ void   **ppFactory
	) = nullptr;

HRESULT __stdcall Hooked_CreateDXGIFactory1(REFIID riid, void **ppFactory1)
{
	LogInfo("*** Hooked_CreateDXGIFactory1 called with riid: %s\n", NameFromIID(riid).c_str());

	// If this happens to be first call from the game, let's make sure to load
	// up our d3d11.dll and the .ini file.
	InitD311();

	// If we are being requested to create a DXGIFactory2, lie and say it's not possible.
	if (riid == __uuidof(IDXGIFactory2) && !G->enable_platform_update)
	{
		LogInfo("  returns E_NOINTERFACE as error for IDXGIFactory2.\n");
		*ppFactory1 = NULL;
		return E_NOINTERFACE;
	}

	// Call original factory, regardless of what they requested, to keep the
	// same expected sequence from their perspective.  (Which includes refcounts)
	HRESULT hr = fnOrigCreateDXGIFactory1(riid, ppFactory1);
	if (FAILED(hr))
	{
		LogInfo("->failed with HRESULT=%x\n", hr);
		return hr;
	}

	if (!fnOrigCreateSwapChain)
		HookCreateSwapChain(*ppFactory1);

	// With the addition of the platform_update, we need to allow for specifically
	// creating a DXGIFactory2 instead of DXGIFactory1.  We want to always upcast
	// the highest supported object for each scenario, to properly suppport
	// QueryInterface and GetParent upcasts.

	IDXGIFactory2* dxgiFactory = reinterpret_cast<IDXGIFactory2*>(*ppFactory1);
	HRESULT res = dxgiFactory->QueryInterface(IID_PPV_ARGS(&dxgiFactory));
	if (SUCCEEDED(res))
	{
		*ppFactory1 = (void*)dxgiFactory;
		LogInfo("  Upcast QueryInterface(IDXGIFactory2) returned result = %x, factory = %p\n", res, dxgiFactory);

		if (!fnOrigCreateSwapChainForHwnd)
			HookCreateSwapChainForHwnd(*ppFactory1);
	}

	LogInfo("  CreateDXGIFactory1 returned factory = %p, result = %x\n", *ppFactory1, hr);
	return hr;
}


// -----------------------------------------------------------------------------

// Some partly obsolete comments, but still maybe worthwhile as thoughts on DXGI.

// The hooks need to be installed late, and cannot be installed during DLLMain, because
// they need to be installed in the COM object vtable itself, and the order cannot be
// defined that early.  Because the documentation says it's not viable at DLLMain time,
// we'll install these hooks at InitD311() time, essentially the first call of D3D11.
// 
// The piece we care about in DXGI is the swap chain, and we don't otherwise have a
// good way to access it.  It can be created directly via DXGI, and not through 
// CreateDeviceAndSwapChain.
//
// Not certain, but it seems likely that we only need to hook a given instance of the
// calls we want, because they are not true objects with attached vtables, they have
// a non-standard vtable/indexing system, and the main differentiator is the object
// passed in as 'this'.  
//
// After much experimentation and study, it seems clear that we should use the in-proc
// version of Deviare. I tried to see if Deviare2 would be a match, but they have a 
// funny event callback mechanism that requires an ATL object connection, and is not
// really suited for same-process operations.  It's really built with separate
// processes in mind.

// For this object, we want to use the CINTERFACE, not the C++ interface.
// The reason is that it allows us easy access to the COM object vtable, which
// we need to hook in order to override the functions.  Once we include dxgi.h
// it will be defined with C headers instead.
//
// This is a little odd, but it's the way that Detours hooks COM objects, and
// thus it seems superior to the Nektra approach of groping the vtable directly
// using constants and void* pointers.
// 
// This is only used for .cpp file here, not the .h file, because otherwise other
// units get compiled with this CINTERFACE, which wrecks their calling out.


// -----------------------------------------------------------------------------
// Functionality removed during refactoring.
// 
// These are here, because our HackerDXGI is only HackerSwapChain now.
// If we want these calls, we'll need to add further hooks here.


//STDMETHODIMP HackerDXGIFactory::MakeWindowAssociation(THIS_
//	HWND WindowHandle,
//	UINT Flags)
//{
//	if (LogFile)
//	{
//		LogInfo("HackerDXGIFactory::MakeWindowAssociation(%s@%p) called with WindowHandle = %p, Flags = %x\n", type_name(this), this, WindowHandle, Flags);
//		if (Flags) LogInfoNoNL("  Flags =");
//		if (Flags & DXGI_MWA_NO_WINDOW_CHANGES) LogInfoNoNL(" DXGI_MWA_NO_WINDOW_CHANGES(no monitoring)");
//		if (Flags & DXGI_MWA_NO_ALT_ENTER) LogInfoNoNL(" DXGI_MWA_NO_ALT_ENTER");
//		if (Flags & DXGI_MWA_NO_PRINT_SCREEN) LogInfoNoNL(" DXGI_MWA_NO_PRINT_SCREEN");
//		if (Flags) LogInfo("\n");
//	}
//
//	if (G->SCREEN_ALLOW_COMMANDS && Flags)
//	{
//		LogInfo("  overriding Flags to allow all window commands\n");
//
//		Flags = 0;
//	}
//	HRESULT hr = mOrigFactory->MakeWindowAssociation(WindowHandle, Flags);
//	LogInfo("  returns result = %x\n", hr);
//
//	return hr;
//}
//
//

//
//static bool FilterRate(int rate)
//{
//	if (!G->FILTER_REFRESH[0]) return false;
//	int i = 0;
//	while (G->FILTER_REFRESH[i] && G->FILTER_REFRESH[i] != rate)
//		++i;
//	return G->FILTER_REFRESH[i] == 0;
//}
//
//STDMETHODIMP HackerDXGIOutput::GetDisplayModeList(THIS_
//	/* [in] */ DXGI_FORMAT EnumFormat,
//	/* [in] */ UINT Flags,
//	/* [annotation][out][in] */
//	__inout  UINT *pNumModes,
//	/* [annotation][out] */
//	__out_ecount_part_opt(*pNumModes, *pNumModes)  DXGI_MODE_DESC *pDesc)
//{
//	LogInfo("HackerDXGIOutput::GetDisplayModeList(%s@%p) called\n", type_name(this), this);
//
//	HRESULT ret = mOrigOutput->GetDisplayModeList(EnumFormat, Flags, pNumModes, pDesc);
//	if (ret == S_OK && pDesc)
//	{
//		for (UINT j = 0; j < *pNumModes; ++j)
//		{
//			int rate = pDesc[j].RefreshRate.Numerator / pDesc[j].RefreshRate.Denominator;
//			if (FilterRate(rate))
//			{
//				LogInfo("  Skipping mode: width=%d, height=%d, refresh rate=%f\n", pDesc[j].Width, pDesc[j].Height,
//					(float)pDesc[j].RefreshRate.Numerator / (float)pDesc[j].RefreshRate.Denominator);
//				// ToDo: Does this work?  I have no idea why setting width and height to 8 would matter.
//				pDesc[j].Width = 8; pDesc[j].Height = 8;
//			}
//			else
//			{
//				LogInfo("  Mode detected: width=%d, height=%d, refresh rate=%f\n", pDesc[j].Width, pDesc[j].Height,
//					(float)pDesc[j].RefreshRate.Numerator / (float)pDesc[j].RefreshRate.Denominator);
//			}
//		}
//	}
//
//	return ret;
//}
//
//STDMETHODIMP HackerDXGIOutput::FindClosestMatchingMode(THIS_
//	/* [annotation][in] */
//	__in  const DXGI_MODE_DESC *pModeToMatch,
//	/* [annotation][out] */
//	__out  DXGI_MODE_DESC *pClosestMatch,
//	/* [annotation][in] */
//	__in_opt  IUnknown *pConcernedDevice)
//{
//	if (pModeToMatch) LogInfo("HackerDXGIOutput::FindClosestMatchingMode(%s@%p) called: width=%d, height=%d, refresh rate=%f\n", type_name(this), this,
//		pModeToMatch->Width, pModeToMatch->Height, (float)pModeToMatch->RefreshRate.Numerator / (float)pModeToMatch->RefreshRate.Denominator);
//
//	HRESULT hr = mOrigOutput->FindClosestMatchingMode(pModeToMatch, pClosestMatch, pConcernedDevice);
//
//	if (pClosestMatch && G->SCREEN_REFRESH >= 0)
//	{
//		pClosestMatch->RefreshRate.Numerator = G->SCREEN_REFRESH;
//		pClosestMatch->RefreshRate.Denominator = 1;
//	}
//	if (pClosestMatch && G->SCREEN_WIDTH >= 0) pClosestMatch->Width = G->SCREEN_WIDTH;
//	if (pClosestMatch && G->SCREEN_HEIGHT >= 0) pClosestMatch->Height = G->SCREEN_HEIGHT;
//	if (pClosestMatch) LogInfo("  returning width=%d, height=%d, refresh rate=%f\n",
//		pClosestMatch->Width, pClosestMatch->Height, (float)pClosestMatch->RefreshRate.Numerator / (float)pClosestMatch->RefreshRate.Denominator);
//
//	LogInfo("  returns hr=%x\n", hr);
//	return hr;
//}
//
//
