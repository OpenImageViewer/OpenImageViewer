#pragma once

#if defined( _MSC_VER )
	//  Microsoft 
	#define GUI_PROVIDER_EXPORT __declspec(dllexport)
	#define GUI_PROVIDER_IMPORT __declspec(dllimport)
#elif defined( __GNUC__ )
	//  GCC
	#define GUI_PROVIDER_EXPORT __attribute__((visibility("default")))
	#define GUI_PROVIDER_IMPORT
#else
	//  Error, compiler must supoport symbol export.
	#pragma error Unknown dynamic link import/export semantics.
#endif

#ifdef GUI_PROVIDER_LIB
#define GUI_PROVIDER_API GUI_PROVIDER_EXPORT  
#else
#define GUI_PROVIDER_API  GUI_PROVIDER_IMPORT 
#endif

enum class ItemChangedMode
{
	  None
	, OnTheFly
	, UserConfirmed
	, Synthesized
};

struct ItemChangedArgs
{
	void* userData;
	const wchar_t* key;
	const wchar_t* val;
	const wchar_t* type;
	ItemChangedMode changedMode;
};

using ItemChangedCallbackType = void (*)(ItemChangedArgs*);

struct GuiCreateParams
{
	const wchar_t* templateFilePath;
	const wchar_t* userSettingsFilePath;
	ItemChangedCallbackType callback;
	void* userData;
};

extern "C"
 {
	 GUI_PROVIDER_API void netsettings_Create(GuiCreateParams*);
	 GUI_PROVIDER_API void netsettings_SetVisible(bool visible);
	 GUI_PROVIDER_API void netsettings_SaveUserSettings();
	 GUI_PROVIDER_API void netsettings_GuiProviderTest();
 }