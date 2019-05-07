// Copyright 2019 The PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdint>

// This template is used in component builds to forward to the real fuzzers
// which are exported from the PDFium shared library.  FUZZER_IMPL is a macro
// defined at build time that contains the name of the real fuzzer.

#if defined(WIN32)
#define IMPORT __declspec(dllimport)
#else
#define IMPORT
#endif

extern "C" IMPORT int FUZZER_IMPL(const uint8_t* data, size_t size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return FUZZER_IMPL(data, size);
}
