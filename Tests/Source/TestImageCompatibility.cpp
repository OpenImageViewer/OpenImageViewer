#include "ImageMagickTestCorpus.h"

#include <catch2/catch_all.hpp>

#include <OIVAppCore/ImageOpenController.h>

#include <ImageLoader.h>
#include <OIVImage/OIVFileImage.h>
#include <TexelFormat.h>
#include <tiffio.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace
{
    OIV::ImageLoadResult LoadWithRealController(const std::wstring& filePath)
    {
        auto imageLoader = std::make_unique<IMCodec::ImageLoader>();
        auto loader      = std::make_unique<OIV::OIVImageFileLoader>(*imageLoader);
        OIV::ImageOpenController controller(std::move(loader));
        return controller.LoadFile(filePath,
                                   IMCodec::PluginTraverseMode::AnyFileType | IMCodec::PluginTraverseMode::AnyPlugin,
                                   OIV::ImageLoadContext{1920, 1080});
    }

    bool IsExpectedLoaded(const OIV::Tests::GeneratedImage& image)
    {
        return image.expectedLoadStatus == "loaded";
    }

    bool IsExpectedUnsupportedImage(const OIV::Tests::GeneratedImage& image)
    {
        return image.expectedLoadStatus == "unsupported";
    }

    bool HasNonZeroDimensions(const IMCodec::ImageSharedPtr& image)
    {
        if (image == nullptr)
            return false;

        if (image->GetWidth() > 0 && image->GetHeight() > 0)
            return true;

        for (uint16_t i = 0; i < image->GetNumSubImages(); ++i)
        {
            if (HasNonZeroDimensions(image->GetSubImage(i)))
                return true;
        }

        return false;
    }

    IMCodec::ImageSharedPtr GetFirstConcreteImage(IMCodec::ImageSharedPtr image)
    {
        if (image != nullptr && image->GetItemType() == IMCodec::ImageItemType::Container &&
            image->GetNumSubImages() > 0)
            return image->GetSubImage(0);

        return image;
    }

    IMCodec::TexelFormat TexelFormatFromName(std::string_view name)
    {
        using enum IMCodec::TexelFormat;

        if (name == "I_R8_G8_B8")
            return I_R8_G8_B8;
        if (name == "I_R16_G16_B16")
            return I_R16_G16_B16;
        if (name == "I_R8_G8_B8_A8")
            return I_R8_G8_B8_A8;
        if (name == "I_R16_G16_B16_A16")
            return I_R16_G16_B16_A16;
        if (name == "F_R32_G32_B32")
            return F_R32_G32_B32;
        if (name == "I_X1")
            return I_X1;
        if (name == "I_X4")
            return I_X4;
        if (name == "I_X8")
            return I_X8;
        if (name == "I_X16")
            return I_X16;
        if (name == "S_X8")
            return S_X8;
        if (name == "S_X16")
            return S_X16;
        if (name == "F_X16")
            return F_X16;
        if (name == "F_X24")
            return F_X24;
        if (name == "F_X32")
            return F_X32;
        if (name == "F_X64")
            return F_X64;

        FAIL("Unsupported expected texel format in ImageMagick corpus: " << name);
        return UNKNOWN;
    }

    class ScopedTempFile
    {
      public:

        explicit ScopedTempFile(std::filesystem::path path) : fPath(std::move(path)) {}
        ScopedTempFile(const ScopedTempFile&)            = delete;
        ScopedTempFile& operator=(const ScopedTempFile&) = delete;

        ~ScopedTempFile()
        {
            std::error_code error;
            std::filesystem::remove(fPath, error);
        }

        const std::filesystem::path& Path() const { return fPath; }

      private:

        std::filesystem::path fPath;
    };

    ScopedTempFile MakeTiffExtraSampleFile()
    {
        static std::atomic_uint64_t nextFileId = 0;

        const auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count();
        const auto fileName  = std::string("oiv-tiff-rgb-two-extra-samples-") + std::to_string(timestamp) + "-" +
                               std::to_string(nextFileId++) + ".tiff";
        return ScopedTempFile(std::filesystem::temp_directory_path() / fileName);
    }

    void WriteRgbTiffWithTwoExtraSamples(const std::filesystem::path& path)
    {
        std::unique_ptr<TIFF, decltype(&TIFFClose)> tiff(TIFFOpen(OIV::Tests::NarrowPath(path).c_str(), "w"),
                                                         TIFFClose);
        REQUIRE(tiff != nullptr);

        constexpr std::uint32_t width                       = 2;
        constexpr std::uint32_t height                      = 2;
        constexpr std::uint16_t samplesPerPixel             = 5;
        const std::array<std::uint16_t, 2> extraSampleTypes = {EXTRASAMPLE_UNASSALPHA, EXTRASAMPLE_UNSPECIFIED};
        const std::array<std::array<std::uint8_t, width * samplesPerPixel>, height> rows = {{
            {255, 0, 0, 64, 1, 0, 255, 0, 128, 2},
            {0, 0, 255, 192, 3, 255, 255, 255, 255, 4},
        }};

        REQUIRE(TIFFSetField(tiff.get(), TIFFTAG_IMAGEWIDTH, width) == 1);
        REQUIRE(TIFFSetField(tiff.get(), TIFFTAG_IMAGELENGTH, height) == 1);
        REQUIRE(TIFFSetField(tiff.get(), TIFFTAG_SAMPLESPERPIXEL, samplesPerPixel) == 1);
        REQUIRE(TIFFSetField(tiff.get(), TIFFTAG_BITSPERSAMPLE, 8) == 1);
        REQUIRE(TIFFSetField(tiff.get(), TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT) == 1);
        REQUIRE(TIFFSetField(tiff.get(), TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT) == 1);
        REQUIRE(TIFFSetField(tiff.get(), TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG) == 1);
        REQUIRE(TIFFSetField(tiff.get(), TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB) == 1);
        REQUIRE(TIFFSetField(tiff.get(), TIFFTAG_COMPRESSION, COMPRESSION_NONE) == 1);
        REQUIRE(TIFFSetField(tiff.get(), TIFFTAG_ROWSPERSTRIP, height) == 1);
        REQUIRE(TIFFSetField(tiff.get(), TIFFTAG_EXTRASAMPLES, static_cast<std::uint16_t>(extraSampleTypes.size()),
                             extraSampleTypes.data()) == 1);

        for (std::uint32_t row = 0; row < height; ++row)
            REQUIRE(TIFFWriteScanline(tiff.get(), const_cast<std::uint8_t*>(rows[row].data()), row, 0) == 1);
    }
}  // namespace

TEST_CASE("ImageLoader decodes ImageMagick compatibility matrix", "[ImageCompatibility][Integration]")
{
    const auto& corpus = OIV::Tests::EnsureImageMagickCorpus();

    for (const auto& image : corpus.validImages)
    {
        DYNAMIC_SECTION(image.format << " " << image.variant << " " << OIV::Tests::NarrowPath(image.path))
        {
            if (!IsExpectedLoaded(image))
                continue;

            INFO("format=" << image.format);
            INFO("variant=" << image.variant);
            INFO("path=" << OIV::Tests::NarrowPath(image.path));

            OIV::ImageLoadResult result;
            try
            {
                result = LoadWithRealController(image.path.wstring());
            }
            catch (const std::exception& exception)
            {
                FAIL("Loading generated image threw: " << exception.what()
                                                       << " path=" << OIV::Tests::NarrowPath(image.path));
            }
            catch (...)
            {
                FAIL("Loading generated image threw an unknown exception: path=" << OIV::Tests::NarrowPath(image.path));
            }

            REQUIRE(result.status == OIV::ImageLoadStatus::Loaded);
            REQUIRE(result.resultCode == ResultCode::RC_Success);
            REQUIRE(result.image != nullptr);

            const auto loadedImage = result.image->GetImage();
            REQUIRE(loadedImage != nullptr);
            REQUIRE(HasNonZeroDimensions(loadedImage));

            const auto concreteImage = GetFirstConcreteImage(loadedImage);
            REQUIRE(concreteImage != nullptr);
            if (image.format == "tiff")
                REQUIRE(concreteImage->GetTexelFormat() != IMCodec::TexelFormat::UNKNOWN);
            if (!image.expectedTexelFormat.empty())
                REQUIRE(concreteImage->GetTexelFormat() == TexelFormatFromName(image.expectedTexelFormat));
            if (!image.expectedSubImageTexelFormats.empty())
            {
                REQUIRE(loadedImage->GetNumSubImages() == image.expectedSubImageTexelFormats.size());
                for (uint16_t i = 0; i < loadedImage->GetNumSubImages(); ++i)
                {
                    REQUIRE(loadedImage->GetSubImage(i)->GetTexelFormat() ==
                            TexelFormatFromName(image.expectedSubImageTexelFormats[i]));
                }
            }
        }
    }
}

TEST_CASE("ImageLoader exposes unmapped TIFF extra samples as subimages", "[ImageCompatibility][Integration]")
{
    const auto tempFile = MakeTiffExtraSampleFile();
    WriteRgbTiffWithTwoExtraSamples(tempFile.Path());

    const auto result = LoadWithRealController(tempFile.Path().wstring());

    REQUIRE(result.status == OIV::ImageLoadStatus::Loaded);
    REQUIRE(result.resultCode == ResultCode::RC_Success);
    REQUIRE(result.image != nullptr);

    const auto loadedImage = result.image->GetImage();
    REQUIRE(loadedImage != nullptr);
    REQUIRE(loadedImage->GetItemType() == IMCodec::ImageItemType::Container);
    REQUIRE(loadedImage->GetNumSubImages() == 3);

    REQUIRE(loadedImage->GetSubImage(0)->GetTexelFormat() == IMCodec::TexelFormat::I_R8_G8_B8);
    REQUIRE(loadedImage->GetSubImage(0)->GetRowPitchInBytes() == 6);
    REQUIRE(loadedImage->GetSubImage(1)->GetTexelFormat() == IMCodec::TexelFormat::I_X8);
    REQUIRE(loadedImage->GetSubImage(1)->GetRowPitchInBytes() == 2);
    REQUIRE(loadedImage->GetSubImage(2)->GetTexelFormat() == IMCodec::TexelFormat::I_X8);
    REQUIRE(loadedImage->GetSubImage(2)->GetRowPitchInBytes() == 2);
}

TEST_CASE("ImageOpenController rejects unsupported ImageMagick corpus variants", "[ImageCompatibility][Integration]")
{
    const auto& corpus         = OIV::Tests::EnsureImageMagickCorpus();
    bool foundUnsupportedImage = false;

    for (const auto& image : corpus.validImages)
    {
        if (!IsExpectedUnsupportedImage(image))
            continue;

        foundUnsupportedImage = true;
        DYNAMIC_SECTION(image.format << " " << image.variant << " " << OIV::Tests::NarrowPath(image.path))
        {
            const auto result = LoadWithRealController(image.path.wstring());

            INFO("unsupported image path=" << OIV::Tests::NarrowPath(image.path));
            REQUIRE(result.status == OIV::ImageLoadStatus::UnsupportedFormat);
            REQUIRE(result.resultCode == ResultCode::RC_FileNotSupported);
        }
    }

    if (!foundUnsupportedImage)
        SUCCEED("ImageMagick corpus has no variants marked as unsupported.");
}

TEST_CASE("ImageOpenController rejects bad ImageMagick corpus files", "[ImageCompatibility][Integration]")
{
    const auto& corpus = OIV::Tests::EnsureImageMagickCorpus();

    for (const auto& image : corpus.badImages)
    {
        DYNAMIC_SECTION(image.variant << " " << OIV::Tests::NarrowPath(image.path))
        {
            const auto result = LoadWithRealController(image.path.wstring());

            INFO("bad image path=" << OIV::Tests::NarrowPath(image.path));
            REQUIRE(result.status != OIV::ImageLoadStatus::Loaded);
            REQUIRE((result.status == OIV::ImageLoadStatus::UnsupportedFormat ||
                     result.status == OIV::ImageLoadStatus::UnknownError));
        }
    }
}
