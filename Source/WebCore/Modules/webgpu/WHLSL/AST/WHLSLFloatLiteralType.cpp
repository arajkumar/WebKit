/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "WHLSLFloatLiteralType.h"

#if ENABLE(WEBGPU)

#include "WHLSLInferTypes.h"
#include "WHLSLNativeTypeDeclaration.h"
#include "WHLSLTypeReference.h"

namespace WebCore {

namespace WHLSL {

namespace AST {

FloatLiteralType::FloatLiteralType(Lexer::Token&& origin, float value)
    : m_value(value)
    , m_preferredType(makeUniqueRef<TypeReference>(WTFMove(origin), "float"_str, TypeArguments()))
{
}

FloatLiteralType::~FloatLiteralType() = default;

FloatLiteralType::FloatLiteralType(FloatLiteralType&&) = default;

FloatLiteralType& FloatLiteralType::operator=(FloatLiteralType&&) = default;

bool FloatLiteralType::canResolve(const Type& type) const
{
    if (!is<NamedType>(type))
        return false;
    auto& namedType = downcast<NamedType>(type);
    if (!is<NativeTypeDeclaration>(namedType))
        return false;
    auto& nativeTypeDeclaration = downcast<NativeTypeDeclaration>(namedType);
    if (!nativeTypeDeclaration.isFloating())
        return false;
    if (!nativeTypeDeclaration.canRepresentFloat()(m_value))
        return false;
    return true;
}

unsigned FloatLiteralType::conversionCost(const UnnamedType& unnamedType) const
{
    if (matches(unnamedType, m_preferredType))
        return 0;
    return 1;
}

FloatLiteralType FloatLiteralType::clone() const
{
    FloatLiteralType result(Lexer::Token(m_preferredType->origin()), m_value);
    if (auto* type = maybeResolvedType())
        result.resolve(type->clone());
    result.m_preferredType = m_preferredType->cloneTypeReference();
    return result;
}


} // namespace AST

}

}

#endif
