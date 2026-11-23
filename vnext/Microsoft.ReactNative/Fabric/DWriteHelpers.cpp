// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "DWriteHelpers.h"

#include <dwrite_3.h>
#include <windows.h>
#include <string>

namespace Microsoft::ReactNative {

winrt::com_ptr<::IDWriteFactory> DWriteFactory() noexcept {
  static winrt::com_ptr<::IDWriteFactory> s_dwriteFactory;
  if (!s_dwriteFactory) {
    winrt::check_hresult(::DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED, __uuidof(s_dwriteFactory), reinterpret_cast<::IUnknown **>(s_dwriteFactory.put())));
  }
  return s_dwriteFactory;
}

// FIT: directory of the app's bundled fonts (next to the .exe: <appdir>\Assets\Fonts\).
// GetModuleFileName works for both packaged (MSIX) and unpackaged apps and matches the
// layout WinMain's RegisterBundledFonts already relies on.
static std::wstring AppFontsDirectory() noexcept {
  wchar_t buf[MAX_PATH]{};
  const DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring exePath(buf, n);
  const auto pos = exePath.find_last_of(L"\\/");
  const std::wstring dir = (pos == std::wstring::npos) ? std::wstring(L".") : exePath.substr(0, pos);
  return dir + L"\\Assets\\Fonts\\";
}

// FIT: a font collection that merges the OS system fonts with the app's bundled TTFs
// (vector-icons + Inter). RNW's WindowsTextLayoutManager passes nullptr to CreateTextFormat,
// which resolves only against the SYSTEM collection - so custom families used to require the
// windows.sharedFonts manifest extension to be injected there. That extension is not permitted
// on every Store account (package-acceptance rejection), so instead we build the collection at
// runtime here and hand it to CreateTextFormat. Built once, cached. Fails closed: on any error
// it returns nullptr and callers fall back to the system collection (previous behavior, no crash).
static winrt::com_ptr<::IDWriteFontCollection> BuildAppFontCollection() noexcept {
  try {
    const auto factory5 = DWriteFactory().as<::IDWriteFactory5>();

    winrt::com_ptr<::IDWriteFontSetBuilder1> builder;
    winrt::check_hresult(factory5->CreateFontSetBuilder(builder.put()));

    // Start from the full system font set so system families still resolve.
    winrt::com_ptr<::IDWriteFontSet> systemSet;
    winrt::check_hresult(factory5->GetSystemFontSet(systemSet.put()));
    winrt::check_hresult(builder->AddFontSet(systemSet.get()));

    // Add every bundled .ttf. Per-file failures are skipped, not fatal.
    const std::wstring dir = AppFontsDirectory();
    const std::wstring pattern = dir + L"*.ttf";
    WIN32_FIND_DATAW ffd{};
    HANDLE hFind = ::FindFirstFileW(pattern.c_str(), &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
      do {
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
          const std::wstring fontPath = dir + ffd.cFileName;
          winrt::com_ptr<::IDWriteFontFile> fontFile;
          if (SUCCEEDED(factory5->CreateFontFileReference(fontPath.c_str(), nullptr, fontFile.put()))) {
            builder->AddFontFile(fontFile.get()); // best effort; ignore per-file HRESULT
          }
        }
      } while (::FindNextFileW(hFind, &ffd));
      ::FindClose(hFind);
    }

    winrt::com_ptr<::IDWriteFontSet> mergedSet;
    winrt::check_hresult(builder->CreateFontSet(mergedSet.put()));
    winrt::com_ptr<::IDWriteFontCollection1> collection1;
    winrt::check_hresult(factory5->CreateFontCollectionFromFontSet(mergedSet.get(), collection1.put()));
    return collection1.as<::IDWriteFontCollection>();
  } catch (...) {
    return nullptr; // fall back to the system collection at call sites
  }
}

// Called from the layout thread (text measure) and the UI thread (paragraph mount). A
// function-local static is initialised exactly once, and C++11 makes concurrent first
// callers wait for it, so no caller can see a half-built (nullptr) collection.
winrt::com_ptr<::IDWriteFontCollection> DWriteAppFontCollection() noexcept {
  static const winrt::com_ptr<::IDWriteFontCollection> s_collection = BuildAppFontCollection();
  return s_collection;
}

} // namespace Microsoft::ReactNative
