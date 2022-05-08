#include <OIVImage/OIVBaseImage.h>
#include "../ApiGlobal.h"
#include <Interfaces/IRenderer.h>

namespace OIV
{
	OIVBaseImage::OIVBaseImage(ImageSource source) : fSource(source) 
	{
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