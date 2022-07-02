//#pragma once
//#include "../../oivlib/oiv/Include/OIVImage/OIVFileImage.h"
//#include "ImageLoader.h"
//
//#include <map>
//#include <thread>
//#include <future>
//#include <functional>
//
//namespace OIV
//{
//	class FileCache
//	{
//		
//	public:
//		using ImageReadyCallbackType = std::function<void(IMCodec::ImageSharedPtr)>;
//
//		FileCache(IMCodec::ImageLoader* imageCodec, ImageReadyCallbackType callback);
//
//		struct FileState
//		{
//			std::wstring imagePath;
//			std::shared_ptr<IMCodec::ImageSharedPtr> imageFile;
//			std::future<ResultCode> loadState;
//		};
//
//		using MapFilePathImage = std::map<std::wstring, FileState>;
//		void Add(const std::wstring& filePath);
//		void Remove(const std::wstring& filePath);
//
//	private: //methods
//		void LoadFileEntryPoint(const std::wstring& filePath, IMCodec::ImageSharedPtr& loadedFile);
//		void LoadedFileEntryPoint(const LoaderType::Result& result);
//		static void OnTaskDone(ULONG_PTR Parameter);
//	private: // member fields
//		using RequestQueue = std::list<std::wstring>;
//		IMCodec::IImageCodec* fImageCodec;
//		ImageReadyCallbackType fCallback;
//		size_t fMaxSimultaniousRequests = 3;
//		RequestQueue fRequestQueue;
//		HANDLE fCreationThreadHandle{};
//		MapFilePathImage fMapPathFile;
//		LoaderType fLoader;
//		std::thread fTestThread;
//
//	};
//}