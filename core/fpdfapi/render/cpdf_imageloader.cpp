// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/render/cpdf_imageloader.h"

#include <utility>

#include "core/fpdfapi/page/cpdf_dib.h"
#include "core/fpdfapi/page/cpdf_image.h"
#include "core/fpdfapi/page/cpdf_imageobject.h"
#include "core/fpdfapi/page/cpdf_transferfunc.h"
#include "core/fpdfapi/render/cpdf_pagerendercache.h"
#include "core/fpdfapi/render/cpdf_rendercontext.h"
#include "core/fpdfapi/render/cpdf_renderstatus.h"
#include "core/fxge/dib/cfx_dibitmap.h"
#include "third_party/base/check.h"

CPDF_ImageLoader::CPDF_ImageLoader() = default;

CPDF_ImageLoader::~CPDF_ImageLoader() = default;

bool CPDF_ImageLoader::Start(const CPDF_ImageObject* pImage,
                             const CPDF_RenderStatus* pRenderStatus,
                             bool bStdCS) {
  m_pCache = pRenderStatus->GetContext()->GetPageCache();
  m_pImageObject = pImage;
  bool ret;
  if (m_pCache) {
    ret = m_pCache->StartGetCachedBitmap(
        m_pImageObject->GetImage(), pRenderStatus->GetFormResource(),
        pRenderStatus->GetPageResource(), bStdCS,
        pRenderStatus->GetGroupFamily(), pRenderStatus->GetLoadMask());
  } else {
    ret = m_pImageObject->GetImage()->StartLoadDIBBase(
        pRenderStatus->GetFormResource(), pRenderStatus->GetPageResource(),
        bStdCS, pRenderStatus->GetGroupFamily(), pRenderStatus->GetLoadMask());
  }
  if (!ret)
    HandleFailure();
  return ret;
}

bool CPDF_ImageLoader::Continue(PauseIndicatorIface* pPause) {
  bool ret = m_pCache ? m_pCache->Continue(pPause)
                      : m_pImageObject->GetImage()->Continue(pPause);
  if (!ret)
    HandleFailure();
  return ret;
}

RetainPtr<CFX_DIBBase> CPDF_ImageLoader::TranslateImage(
    RetainPtr<CPDF_TransferFunc> pTransferFunc) {
  DCHECK(pTransferFunc);
  DCHECK(!pTransferFunc->GetIdentity());
  m_pBitmap = pTransferFunc->TranslateImage(std::move(m_pBitmap));
  if (m_bCached && m_pMask)
    m_pMask = m_pMask->Realize();
  m_bCached = false;
  return m_pBitmap;
}

void CPDF_ImageLoader::HandleFailure() {
  if (m_pCache) {
    m_bCached = true;
    m_pBitmap = m_pCache->DetachCurBitmap();
    m_pMask = m_pCache->DetachCurMask();
    m_MatteColor = m_pCache->GetCurMatteColor();
    return;
  }
  RetainPtr<CPDF_Image> pImage = m_pImageObject->GetImage();
  m_bCached = false;
  m_pBitmap = pImage->DetachBitmap();
  m_pMask = pImage->DetachMask();
  m_MatteColor = pImage->GetMatteColor();
}
