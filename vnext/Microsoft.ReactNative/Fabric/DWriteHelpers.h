
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include <dwrite.h>

namespace Microsoft::ReactNative {

winrt::com_ptr<::IDWriteFactory> DWriteFactory() noexcept;

// FIT: system fonts + the app's bundled Assets\Fonts TTFs, merged into one collection so
// custom families (vector-icons, Inter) resolve without the windows.sharedFonts manifest
// extension. Built once; returns nullptr on failure (callers fall back to the system collection).
winrt::com_ptr<::IDWriteFontCollection> DWriteAppFontCollection() noexcept;

} // namespace Microsoft::ReactNative
