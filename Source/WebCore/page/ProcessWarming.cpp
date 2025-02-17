/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ProcessWarming.h"

#include "CSSDefaultStyleSheets.h"
#include "CommonVM.h"
#include "Font.h"
#include "FontCache.h"
#include "FontCascadeDescription.h"
#include "HTMLNames.h"
#include "MathMLNames.h"
#include "MediaFeatureNames.h"
#include "QualifiedName.h"
#include "SVGNames.h"
#include "Settings.h"
#include "TelephoneNumberDetector.h"
#include "WebKitFontFamilyNames.h"
#include "XLinkNames.h"
#include "XMLNSNames.h"
#include "XMLNames.h"

namespace WebCore {

void ProcessWarming::initializeNames()
{
    AtomString::init();
    HTMLNames::init();
    QualifiedName::init();
    MediaFeatureNames::init();
    SVGNames::init();
    XLinkNames::init();
    MathMLNames::init();
    XMLNSNames::init();
    XMLNames::init();
    WebKitFontFamilyNames::init();
}
    
void ProcessWarming::prewarmGlobally()
{
    initializeNames();
    
    // Initializes default font families.
    Settings::create(nullptr);
    
    // Prewarms user agent stylesheet.
    CSSDefaultStyleSheets::loadFullDefaultStyle();
    
    // Prewarms JS VM.
    commonVM();

#if USE_PLATFORM_SYSTEM_FALLBACK_LIST
    // Cache system UI font fallbacks. Almost every web process needs these.
    // Initializing one size is sufficient to warm CoreText caches.
    FontCascadeDescription systemFontDescription;
    systemFontDescription.setOneFamily("system-ui");
    systemFontDescription.setComputedSize(11);
    systemFontDescription.effectiveFamilyCount();
#endif

#if ENABLE(TELEPHONE_NUMBER_DETECTION)
    TelephoneNumberDetector::isSupported();
#endif
}

WebCore::PrewarmInformation ProcessWarming::collectPrewarmInformation()
{
    return { FontCache::singleton().collectPrewarmInformation() };
}

void ProcessWarming::prewarmWithInformation(const PrewarmInformation& prewarmInfo)
{
    FontCache::singleton().prewarm(prewarmInfo.fontCache);
}

}
