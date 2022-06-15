#include <OIVImage/OIVBaseImage.h>
#include "../ApiGlobal.h"
#include <Interfaces/IRenderer.h>

namespace OIV
{
	OIVBaseImage::OIVBaseImage(ImageSource source) : fSource(source)
	{
		// Renderer and the unique id provider are not thread safe 
		// use critical section upon object creation
		std::lock_guard<std::mutex> lock(fRendererMutex);
		fObjectId = fUniqueIdProvider.Acquire();
		ApiGlobal::sPictureRenderer->AddRenderable(this);
	}
	OIVBaseImage::OIVBaseImage(ImageSource source, IMCodec::ImageSharedPtr image) : OIVBaseImage(source)
	{
		SetUnderlyingImage(image);
	}
	void OIVBaseImage::SetUnderlyingImage(IMCodec::ImageSharedPtr image) 
	{ 
		fImage = image; 
		fIsImageDirty = true;
	}

	OIVBaseImage::~OIVBaseImage()
	{
		ApiGlobal::sPictureRenderer->RemoveRenderable(this);
	}
	
}