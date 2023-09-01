#pragma once
#include "Helpers/OIVHelper.h"
#include "LabelManager.h"

namespace OIV
{
    class VirtualStatusBar
    {
    public:
        using RefreshCallback = std::function<void()>;
        

        VirtualStatusBar(LabelManager* labelManager, RefreshCallback callback)
        {
            fLabelManager = labelManager;
            fRefreshCallback = callback;
        }

		OIVTextImage* GetOrCreateLabel(const std::string& labelName)
		{
			auto label = fMapLabels.find(labelName);
			if (label == fMapLabels.end())
			{
				OIVTextImage* texelValue = fLabelManager->GetOrCreateTextLabel(labelName);
                texelValue->SetVisible(fVisible);
                texelValue->SetBackgroundColor(LLUtils::Color(0, 0, 0, 192));
                texelValue->SetFontPath(LabelManager::sFixedFontPath);
                texelValue->SetLineEndFixedWidth(true);
                texelValue->SetTextRenderMode(FreeType::RenderMode::Antialiased);
                texelValue->SetFontSize(11);
                texelValue->SetTextColor({ 170,170,170,255 });
                texelValue->SetOutlineWidth(0);
				label = fMapLabels.emplace(labelName, texelValue).first;
			}

			return label->second;
		}


        void RepositionLabels()
        {
            const LLUtils::PointF64 sizef = static_cast<LLUtils::PointF64>(fClientSize);
            
            for (auto [name,text] : fMapLabels)
            {
                // labels placement logic, currently hard coded.
                //TOOD: make it dynamic
                if (text->GetImage() == nullptr)
                    text->Create();

                if (text->GetImage() != nullptr &&  text->GetOpacity() > 0.0 && text->GetVisible() ) // if visible
                {
                    if (name == "texelValue")
                        text->SetPosition({ sizef.x - text->GetImage()->GetWidth() - 10 ,sizef.y - text->GetImage()->GetHeight() - 5 });
                    else if (name == "imageDescription")
                        text->SetPosition({ 10 ,sizef.y - text->GetImage()->GetHeight()- 5 });
                    else if (name == "texelPos")
                        text->SetPosition({ (sizef.x - text->GetImage()->GetWidth()) / 2  ,sizef.y - text->GetImage()->GetHeight()- 5 });

                }
            }

            fRefreshCallback();
        }

        void ClientSizeChanged(LLUtils::PointI32& size)
        {
            fClientSize = size;
            if (GetVisible())
                RepositionLabels();
        }

        void SetText(std::string elementName, const OIVString& text)
        {
            OIVTextImage* texelValue = GetOrCreateLabel(elementName);
            texelValue->SetText(text);

            if (GetVisible())
            {
                RepositionLabels(); // size of text is most likely changed - reposition.
            }
        }

        bool GetVisible() const
        {
            return fVisible;
        }

        void SetVisible(bool visible)
        {
            if (fVisible != visible)
            {
                fVisible = visible;
                for (auto [name, text] : fMapLabels)
                {
                    text->SetVisible(fVisible);
                }

                RepositionLabels();
            }
        }
        /// <summary>
        /// Sets the opacity of a label and return true if update is needed.
        /// </summary>
        /// <param name="elementName"></param>
        /// <param name="opacity"></param>
        /// <returns></returns>
        bool SetOpacity(std::string elementName, double opacity)
        {
            OIVTextImage* label = GetOrCreateLabel(elementName);
            if (label->GetOpacity() != opacity)
            {
                label->SetOpacity(opacity);
                return true;
            }

            return false;
        }

    private:

        LLUtils::PointI32 fClientSize = LLUtils::PointI32::Zero;
        std::map<std::string, OIVTextImage*> fMapLabels;
        LabelManager* fLabelManager;
        RefreshCallback fRefreshCallback;
        bool fVisible = false;

    };
}
