#include "Viewer.h"

#include <ImageUtil/ImageUtil.h>

#include <LLUtils/Exception.h>

#include <gio/gio.h>
#include <gtk/gtk.h>

#include <cstring>
#include <limits>
#include <string>

namespace
{
    IMCodec::ImageSharedPtr CreateImageFromPixbuf(GdkPixbuf* pixbuf)
    {
        if (pixbuf == nullptr || gdk_pixbuf_get_colorspace(pixbuf) != GDK_COLORSPACE_RGB ||
            gdk_pixbuf_get_bits_per_sample(pixbuf) != 8)
        {
            return nullptr;
        }

        const int width    = gdk_pixbuf_get_width(pixbuf);
        const int height   = gdk_pixbuf_get_height(pixbuf);
        const int channels = gdk_pixbuf_get_n_channels(pixbuf);
        if (width <= 0 || height <= 0 || (channels != 3 && channels != 4))
            return nullptr;

        const int sourceRowStride = gdk_pixbuf_get_rowstride(pixbuf);
        if (sourceRowStride <= 0)
            return nullptr;

        const size_t targetPitch = static_cast<size_t>(width) * static_cast<size_t>(channels);
        const size_t sourcePitch = static_cast<size_t>(sourceRowStride);
        if (targetPitch > std::numeric_limits<uint32_t>::max() || sourcePitch < targetPitch ||
            static_cast<size_t>(height) > std::numeric_limits<size_t>::max() / targetPitch)
        {
            return nullptr;
        }

        using namespace IMCodec;
        auto imageItem                           = std::make_shared<ImageItem>();
        imageItem->itemType                      = ImageItemType::Image;
        imageItem->descriptor.width              = static_cast<uint32_t>(width);
        imageItem->descriptor.height             = static_cast<uint32_t>(height);
        imageItem->descriptor.rowPitchInBytes    = static_cast<uint32_t>(targetPitch);
        imageItem->descriptor.texelFormatStorage = channels == 4 ? TexelFormat::I_R8_G8_B8_A8 : TexelFormat::I_R8_G8_B8;
        imageItem->descriptor.texelFormatDecompressed = imageItem->descriptor.texelFormatStorage;
        imageItem->data.Allocate(static_cast<size_t>(height) * imageItem->descriptor.rowPitchInBytes);

        const auto* sourcePixels = reinterpret_cast<const std::byte*>(gdk_pixbuf_get_pixels(pixbuf));
        if (sourcePixels == nullptr)
            return nullptr;
        for (int row = 0; row < height; ++row)
        {
            std::memcpy(imageItem->data.data() + static_cast<size_t>(row) * targetPitch,
                        sourcePixels + static_cast<size_t>(row) * sourcePitch, targetPitch);
        }
        return std::make_shared<Image>(imageItem, ImageItemType::Unknown);
    }
}  // namespace

namespace OIV
{
    void Viewer::DeleteOpenedFile(bool permanently)
    {
        const auto fileNameToRemove = GetOpenedFileName();
        GFile* file                 = g_file_new_for_path(fileNameToRemove.c_str());
        GError* error               = nullptr;
        fRequestedFileForRemoval    = fileNameToRemove;
        const gboolean removed      = permanently ? g_file_delete(file, nullptr, &error)
                                                  : g_file_trash(file, nullptr, &error);
        g_object_unref(file);

        if (removed != FALSE)
        {
            ProcessRemovalOfOpenedFile(fileNameToRemove);
        }
        else
        {
            fRequestedFileForRemoval.clear();
            std::string message = permanently ? "Unable to delete file" : "Unable to move file to trash";
            if (error != nullptr)
            {
                message += ": ";
                message += error->message;
                g_error_free(error);
            }
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, message);
        }
    }

    ClipboardDataType Viewer::PasteFromClipBoard()
    {
        if (gtk_init_check(nullptr, nullptr) == FALSE)
            return ClipboardDataType::None;

        GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        if (GdkPixbuf* pixbuf = gtk_clipboard_wait_for_image(clipboard); pixbuf != nullptr)
        {
            auto image = CreateImageFromPixbuf(pixbuf);
            g_object_unref(pixbuf);
            if (image != nullptr)
            {
                LoadOivImage(ViewerImage{.pixels = std::move(image), .source = ImageSource::Clipboard});
                return ClipboardDataType::Image;
            }
        }

        if (gchar* text = gtk_clipboard_wait_for_text(clipboard); text != nullptr)
        {
            const LLUtils::native_string_type nativeText(text);
            g_free(text);
            if (!nativeText.empty())
            {
                LoadClipboardText(nativeText);
                return ClipboardDataType::Text;
            }
        }
        return ClipboardDataType::None;
    }

    bool Viewer::SetClipboardImage(IMCodec::ImageSharedPtr image)
    {
        if (gtk_init_check(nullptr, nullptr) == FALSE)
            return false;

        auto clipboardImage = IMUtil::ImageUtil::ConvertImageWithNormalization(image,
                                                                               IMCodec::TexelFormat::I_R8_G8_B8_A8,
                                                                               false);
        if (clipboardImage == nullptr)
            return false;

        if (clipboardImage->GetWidth() > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
            clipboardImage->GetHeight() > static_cast<uint32_t>(std::numeric_limits<int>::max()))
        {
            return false;
        }

        GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, static_cast<int>(clipboardImage->GetWidth()),
                                           static_cast<int>(clipboardImage->GetHeight()));
        if (pixbuf == nullptr)
            return false;

        const int destinationRowStride = gdk_pixbuf_get_rowstride(pixbuf);
        const size_t rowBytes          = static_cast<size_t>(clipboardImage->GetWidth()) * 4;
        const size_t sourcePitch       = clipboardImage->GetRowPitchInBytes();
        const auto* sourcePixels       = clipboardImage->GetBuffer();
        auto* destinationPixels        = reinterpret_cast<std::byte*>(gdk_pixbuf_get_pixels(pixbuf));
        if (destinationRowStride <= 0 || static_cast<size_t>(destinationRowStride) < rowBytes ||
            sourcePitch < rowBytes || sourcePixels == nullptr || destinationPixels == nullptr)
        {
            g_object_unref(pixbuf);
            return false;
        }
        const size_t destinationPitch = static_cast<size_t>(destinationRowStride);
        for (uint32_t row = 0; row < clipboardImage->GetHeight(); ++row)
        {
            std::memcpy(destinationPixels + static_cast<size_t>(row) * destinationPitch,
                        sourcePixels + static_cast<size_t>(row) * sourcePitch, rowBytes);
        }

        GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        gtk_clipboard_set_image(clipboard, pixbuf);
        gtk_clipboard_set_can_store(clipboard, nullptr, 0);
        gtk_clipboard_store(clipboard);
        g_object_unref(pixbuf);
        return true;
    }

    bool Viewer::ConfirmFileReload(const LLUtils::native_string_type& requestedFile)
    {
        bool accepted = false;
        if (gtk_init_check(nullptr, nullptr) != FALSE)
        {
            GtkWidget* dialog = gtk_message_dialog_new(nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
                                                       GTK_BUTTONS_YES_NO, "Reload the file: %s?",
                                                       requestedFile.c_str());
            accepted          = gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES;
            gtk_widget_destroy(dialog);
        }
        return accepted;
    }
}  // namespace OIV
