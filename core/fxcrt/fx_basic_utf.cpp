// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcrt/fx_basic.h"

#include <vector>

namespace {

class CFX_UTF8Encoder {
 public:
  CFX_UTF8Encoder() {}
  ~CFX_UTF8Encoder() {}

  void Input(wchar_t unicodeAsWchar) {
    uint32_t unicode = static_cast<uint32_t>(unicodeAsWchar);
    if (unicode < 0x80) {
      m_Buffer.push_back(unicode);
    } else {
      if (unicode >= 0x80000000)
        return;

      int nbytes = 0;
      if (unicode < 0x800)
        nbytes = 2;
      else if (unicode < 0x10000)
        nbytes = 3;
      else if (unicode < 0x200000)
        nbytes = 4;
      else if (unicode < 0x4000000)
        nbytes = 5;
      else
        nbytes = 6;

      static uint8_t prefix[] = {0xc0, 0xe0, 0xf0, 0xf8, 0xfc};
      int order = 1 << ((nbytes - 1) * 6);
      int code = unicodeAsWchar;
      m_Buffer.push_back(prefix[nbytes - 2] | (code / order));
      for (int i = 0; i < nbytes - 1; i++) {
        code = code % order;
        order >>= 6;
        m_Buffer.push_back(0x80 | (code / order));
      }
    }
  }

  // The data returned by GetResult() is invalidated when this is modified by
  // appending any data.
  CFX_ByteStringC GetResult() const {
    return CFX_ByteStringC(m_Buffer.data(), m_Buffer.size());
  }

 private:
  std::vector<uint8_t> m_Buffer;
};

}  // namespace

void CFX_UTF8Decoder::Clear() {
  m_Buffer.Clear();
  m_PendingBytes = 0;
}

void CFX_UTF8Decoder::AppendCodePoint(uint32_t ch) {
  m_Buffer.AppendChar(static_cast<wchar_t>(ch));
}

void CFX_UTF8Decoder::Input(uint8_t byte) {
  if (byte < 0x80) {
    m_PendingBytes = 0;
    m_Buffer.AppendChar(byte);
  } else if (byte < 0xc0) {
    if (m_PendingBytes == 0) {
      return;
    }
    m_PendingBytes--;
    m_PendingChar |= (byte & 0x3f) << (m_PendingBytes * 6);
    if (m_PendingBytes == 0) {
      AppendCodePoint(m_PendingChar);
    }
  } else if (byte < 0xe0) {
    m_PendingBytes = 1;
    m_PendingChar = (byte & 0x1f) << 6;
  } else if (byte < 0xf0) {
    m_PendingBytes = 2;
    m_PendingChar = (byte & 0x0f) << 12;
  } else if (byte < 0xf8) {
    m_PendingBytes = 3;
    m_PendingChar = (byte & 0x07) << 18;
  } else if (byte < 0xfc) {
    m_PendingBytes = 4;
    m_PendingChar = (byte & 0x03) << 24;
  } else if (byte < 0xfe) {
    m_PendingBytes = 5;
    m_PendingChar = (byte & 0x01) << 30;
  }
}

CFX_ByteString FX_UTF8Encode(const CFX_WideStringC& wsStr) {
  FX_STRSIZE len = wsStr.GetLength();
  const wchar_t* pStr = wsStr.unterminated_c_str();
  CFX_UTF8Encoder encoder;
  while (len-- > 0)
    encoder.Input(*pStr++);

  return CFX_ByteString(encoder.GetResult());
}
