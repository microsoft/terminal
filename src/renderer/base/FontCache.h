// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <til/mutex.h>

namespace Microsoft::Console::Render::FontCache
{
    namespace details
    {
        inline wil::com_ptr<IDWriteFontCollection> getFontCollection()
        {
            wil::com_ptr<IDWriteFactory> factory;
            THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(factory), reinterpret_cast<::IUnknown**>(factory.addressof())));

            wil::com_ptr<IDWriteFontCollection> systemFontCollection;
            THROW_IF_FAILED(factory->GetSystemFontCollection(systemFontCollection.addressof(), FALSE));

            if constexpr (Feature_NearbyFontLoading::IsEnabled())
            {
                // IDWriteFactory5 is supported since Windows 10, build 15021.
                const auto factory5 = factory.try_query<IDWriteFactory5>();
                if (!factory5)
                {
                    return systemFontCollection;
                }

                std::vector<wil::com_ptr<IDWriteFontFile>> nearbyFontFiles;

                const std::filesystem::path module{ wil::GetModuleFileNameW<std::wstring>(nullptr) };
                const auto folder{ module.parent_path() };

                for (const auto& p : std::filesystem::directory_iterator(folder))
                {
                    if (til::ends_with(p.path().native(), L".ttf"))
                    {
                        wil::com_ptr<IDWriteFontFile> fontFile;
                        if (SUCCEEDED_LOG(factory5->CreateFontFileReference(p.path().c_str(), nullptr, fontFile.addressof())))
                        {
                            nearbyFontFiles.emplace_back(std::move(fontFile));
                        }
                    }
                }

                if (nearbyFontFiles.empty())
                {
                    return systemFontCollection;
                }

                wil::com_ptr<IDWriteFontSet> systemFontSet;
                // IDWriteFontCollection1 is supported since Windows 7.
                THROW_IF_FAILED(systemFontCollection.query<IDWriteFontCollection1>()->GetFontSet(systemFontSet.addressof()));

                wil::com_ptr<IDWriteFontSetBuilder1> fontSetBuilder;
                THROW_IF_FAILED(factory5->CreateFontSetBuilder(fontSetBuilder.addressof()));

                for (const auto& file : nearbyFontFiles)
                {
                    LOG_IF_FAILED(fontSetBuilder->AddFontFile(file.get()));
                }

                // IDWriteFontSetBuilder ignores any families that have already been added.
                // By adding the system font collection last, we ensure our nearby fonts take precedence.
                THROW_IF_FAILED(fontSetBuilder->AddFontSet(systemFontSet.get()));

                wil::com_ptr<IDWriteFontSet> fontSet;
                THROW_IF_FAILED(fontSetBuilder->CreateFontSet(fontSet.addressof()));

                wil::com_ptr<IDWriteFontCollection1> fontCollection;
                THROW_IF_FAILED(factory5->CreateFontCollectionFromFontSet(fontSet.get(), fontCollection.addressof()));

                return std::move(fontCollection);
            }
            else
            {
                return systemFontCollection;
            }
        }
    }

    inline wil::com_ptr<IDWriteFontCollection> GetCached()
    {
        static til::shared_mutex<wil::com_ptr<IDWriteFontCollection>> cachedCollection;

        const auto guard = cachedCollection.lock();
        if (!*guard)
        {
            *guard = details::getFontCollection();
        }
        return *guard;
    }
}
