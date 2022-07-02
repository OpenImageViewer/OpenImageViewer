//#include "FileCache.h"
//#include <thread>
//#include <AclAPI.h>
//
//namespace OIV
//{
//	void FileCache::Add(const std::wstring& filePath)
//	{
//		if (fMapPathFile.size() >= fMaxSimultaniousRequests)
//		{
//			fRequestQueue.push_front(filePath);
//		}
//
//		auto it = fMapPathFile.find(filePath);
//
//		if (it != fMapPathFile.end())
//			LL_EXCEPTION(LLUtils::Exception::ErrorCode::DuplicateItem, "Can not add two identical files");
//
//		auto it_new = fMapPathFile.insert(std::make_pair(filePath, FileState{filePath}));
//		auto& fileState = it_new.first->second;
//		fLoader.AddRequest(filePath);
//	}
//
//	FileCache::FileCache(IMCodec::ImageLoader* imageCodec, ImageReadyCallbackType callback) : fCallback(callback),
//	 fLoader(std::bind(&FileCache::LoadFileEntryPoint, this, std::placeholders::_1, std::placeholders::_2),
//									 std::bind(&FileCache::LoadedFileEntryPoint, this, std::placeholders::_1))
//	{
//		fLoader.SetUsePolling(false);
//	}
//
//	
//	void FileCache::LoadFileEntryPoint(const std::wstring& filePath, IMCodec::ImageSharedPtr& loadedFile)
//	{
//	//fImageCodec->	
//	//	auto file = std::make_shared<OIVFileImage>(filePath);
//	//	//file->Load(IMCodec::ImageLoaderFlags::OnlyRegisteredExtension);
//	//	loadedFile = file;
//	}
//	void FileCache::LoadedFileEntryPoint(const LoaderType::Result& result)
//	{
//		fCallback(result.response);
//	}
//
//	void FileCache::Remove(const std::wstring& filePath)
//	{
//		auto it = fMapPathFile.find(filePath);
//
//		if (it != fMapPathFile.end())
//			LL_EXCEPTION(LLUtils::Exception::ErrorCode::DuplicateItem, "Can not remove non existent file");
//
//		auto it_new = fMapPathFile.erase(it);
//	}
//}