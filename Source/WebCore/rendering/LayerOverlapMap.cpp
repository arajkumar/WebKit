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
#include "LayerOverlapMap.h"
#include "RenderLayer.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

struct RectList {
    Vector<LayoutRect> rects;
    LayoutRect boundingRect;
    
    void append(const LayoutRect& rect)
    {
        rects.append(rect);
        boundingRect.unite(rect);
    }

    void append(const RectList& rectList)
    {
        rects.appendVector(rectList.rects);
        boundingRect.unite(rectList.boundingRect);
    }
    
    bool intersects(const LayoutRect& rect) const
    {
        if (!rects.size() || !rect.intersects(boundingRect))
            return false;

        for (const auto& currentRect : rects) {
            if (currentRect.intersects(rect))
                return true;
        }
        return false;
    }
};

static TextStream& operator<<(TextStream& ts, const RectList& rectList)
{
    ts << "bounds " << rectList.boundingRect << " (" << rectList.rects << " rects)";
    return ts;
}

// Used to store overlap rects in a way that takes overflow into account.
// It stores a tree whose nodes are layers with composited scrolling. The tree is built lazily as layers are added whose containing block
// chains contain composited scrollers. The tree always starts at the root layer.
// Checking for overlap involves finding the node for the clipping layer enclosing the given layer (or the root),
// and comparing against the bounds of earlier siblings.
class OverlapMapContainer {
public:
    OverlapMapContainer(const RenderLayer& rootLayer)
        : m_rootScope(rootLayer)
    {
    }

    // Layers are added in z-order, lazily creating clipping scopes as necessary.
    void add(const RenderLayer&, const LayoutRect& bounds, const Vector<LayerOverlapMap::LayerAndBounds>& enclosingClippingLayers);
    bool overlapsLayers(const RenderLayer&, const LayoutRect& bounds, const Vector<LayerOverlapMap::LayerAndBounds>& enclosingClippingLayers) const;
    void append(std::unique_ptr<OverlapMapContainer>&&);
    
    String dump(unsigned) const;

private:
    struct ClippingScope {
        ClippingScope(const RenderLayer& inLayer)
            : layer(inLayer)
        {
        }

        ClippingScope(const LayerOverlapMap::LayerAndBounds& layerAndBounds)
            : layer(layerAndBounds.layer)
            , bounds(layerAndBounds.bounds)
        {
        }

        ClippingScope* childWithLayer(const RenderLayer& layer) const
        {
            for (auto& child : children) {
                if (&child.layer == &layer)
                    return const_cast<ClippingScope*>(&child);
            }
            return nullptr;
        }

        ClippingScope* addChildWithLayerAndBounds(const LayerOverlapMap::LayerAndBounds& layerAndBounds)
        {
            children.append({ layerAndBounds });
            return &children.last();
        }

        ClippingScope* addChild(const ClippingScope& child)
        {
            children.append(child);
            return &children.last();
        }

        void appendRect(const LayoutRect& bounds)
        {
            rectList.append(bounds);
        }

        const RenderLayer& layer;
        LayoutRect bounds; // Bounds of the composited clip.
        Vector<ClippingScope> children;
        RectList rectList;
    };

    static ClippingScope* clippingScopeContainingLayerChildRecursive(const ClippingScope& currNode, const RenderLayer& layer)
    {
        for (auto& child : currNode.children) {
            if (&layer == &child.layer)
                return const_cast<ClippingScope*>(&currNode);

            if (auto* foundNode = clippingScopeContainingLayerChildRecursive(child, layer))
                return foundNode;
        }

        return nullptr;
    }

    ClippingScope* scopeContainingLayer(const RenderLayer& layer) const
    {
        return clippingScopeContainingLayerChildRecursive(m_rootScope, layer);
    }
    
    static void mergeClippingScopesRecursive(const ClippingScope& sourceScope, ClippingScope& destScope);

    ClippingScope* ensureClippingScopeForLayers(const Vector<LayerOverlapMap::LayerAndBounds>& enclosingClippingLayers);
    ClippingScope* findClippingScopeForLayers(const Vector<LayerOverlapMap::LayerAndBounds>& enclosingClippingLayers) const;

    void recursiveOutputToStream(TextStream&, const ClippingScope&, unsigned depth) const;

    const ClippingScope& rootScope() const { return m_rootScope; }
    ClippingScope& rootScope() { return m_rootScope; }

    ClippingScope m_rootScope;
};

void OverlapMapContainer::add(const RenderLayer&, const LayoutRect& bounds, const Vector<LayerOverlapMap::LayerAndBounds>& enclosingClippingLayers)
{
    auto* layerScope = ensureClippingScopeForLayers(enclosingClippingLayers);
    layerScope->appendRect(bounds);
}

bool OverlapMapContainer::overlapsLayers(const RenderLayer&, const LayoutRect& bounds, const Vector<LayerOverlapMap::LayerAndBounds>& enclosingClippingLayers) const
{
    if (m_rootScope.rectList.intersects(bounds))
        return true;

    if (m_rootScope.children.isEmpty())
        return false;

    // Find the ClippingScope for which this layer is a child.
    auto* clippingScope = findClippingScopeForLayers(enclosingClippingLayers);
    if (!clippingScope)
        return false;

    if (clippingScope->rectList.intersects(bounds))
        return true;

    // FIXME: In some cases do we have to walk up the ancestor clipping scope chain?
    return false;
}

void OverlapMapContainer::mergeClippingScopesRecursive(const ClippingScope& sourceScope, ClippingScope& destScope)
{
    ASSERT(&sourceScope.layer == &destScope.layer);
    destScope.rectList.append(sourceScope.rectList);

    for (auto& sourceChildScope : sourceScope.children) {
        ClippingScope* destChild = destScope.childWithLayer(sourceChildScope.layer);
        if (destChild) {
            destChild->rectList.append(sourceChildScope.rectList);
            mergeClippingScopesRecursive(sourceChildScope, *destChild);
        } else {
            // New child, just copy the whole subtree.
            destScope.addChild(sourceScope);
        }
    }
}

void OverlapMapContainer::append(std::unique_ptr<OverlapMapContainer>&& otherContainer)
{
    mergeClippingScopesRecursive(otherContainer->rootScope(), m_rootScope);
}

OverlapMapContainer::ClippingScope* OverlapMapContainer::ensureClippingScopeForLayers(const Vector<LayerOverlapMap::LayerAndBounds>& enclosingClippingLayers)
{
    ASSERT(enclosingClippingLayers.size());
    ASSERT(enclosingClippingLayers[0].layer.isRenderViewLayer());

    auto* currScope = &m_rootScope;
    for (unsigned i = 1; i < enclosingClippingLayers.size(); ++i) {
        auto& scopeLayerAndBounds = enclosingClippingLayers[i];
        auto* childScope = currScope->childWithLayer(scopeLayerAndBounds.layer);
        if (!childScope) {
            currScope = currScope->addChildWithLayerAndBounds(scopeLayerAndBounds);
            break;
        }

        currScope = childScope;
    }

    return const_cast<ClippingScope*>(currScope);
}

OverlapMapContainer::ClippingScope* OverlapMapContainer::findClippingScopeForLayers(const Vector<LayerOverlapMap::LayerAndBounds>& enclosingClippingLayers) const
{
    ASSERT(enclosingClippingLayers.size());
    ASSERT(enclosingClippingLayers[0].layer.isRenderViewLayer());

    const auto* currScope = &m_rootScope;
    for (unsigned i = 1; i < enclosingClippingLayers.size(); ++i) {
        auto& scopeLayerAndBounds = enclosingClippingLayers[i];
        auto* childScope = currScope->childWithLayer(scopeLayerAndBounds.layer);
        if (!childScope)
            return nullptr;

        currScope = childScope;
    }

    return const_cast<ClippingScope*>(currScope);
}

void OverlapMapContainer::recursiveOutputToStream(TextStream& ts, const ClippingScope& scope, unsigned depth) const
{
    ts << "\n" << indent << TextStream::Repeat { 2 * depth, ' ' } << " scope for layer " << &scope.layer << " rects " << scope.rectList;
    for (auto& childScope : scope.children)
        recursiveOutputToStream(ts, childScope, depth + 1);
}

String OverlapMapContainer::dump(unsigned indent) const
{
    TextStream multilineStream;
    multilineStream.increaseIndent(indent);
    multilineStream << "overlap container - root scope layer " <<  &m_rootScope.layer << " rects " << m_rootScope.rectList;

    for (auto& childScope : m_rootScope.children)
        recursiveOutputToStream(multilineStream, childScope, 1);

    return multilineStream.release();
}

LayerOverlapMap::LayerOverlapMap(const RenderLayer& rootLayer)
    : m_geometryMap(UseTransforms)
    , m_rootLayer(rootLayer)
{
    // Begin assuming the root layer will be composited so that there is
    // something on the stack. The root layer should also never get an
    // popCompositingContainer call.
    pushCompositingContainer();
}

LayerOverlapMap::~LayerOverlapMap() = default;

void LayerOverlapMap::add(const RenderLayer& layer, const LayoutRect& bounds, const Vector<LayerAndBounds>& enclosingClippingLayers)
{
    // Layers do not contribute to overlap immediately--instead, they will
    // contribute to overlap as soon as their composited ancestor has been
    // recursively processed and popped off the stack.
    ASSERT(m_overlapStack.size() >= 2);
    m_overlapStack[m_overlapStack.size() - 2]->add(layer, bounds, enclosingClippingLayers);
    
    m_isEmpty = false;
}

bool LayerOverlapMap::overlapsLayers(const RenderLayer& layer, const LayoutRect& bounds, const Vector<LayerAndBounds>& enclosingClippingLayers) const
{
    return m_overlapStack.last()->overlapsLayers(layer, bounds, enclosingClippingLayers);
}

void LayerOverlapMap::pushCompositingContainer()
{
    m_overlapStack.append(std::make_unique<OverlapMapContainer>(m_rootLayer));
}

void LayerOverlapMap::popCompositingContainer()
{
    m_overlapStack[m_overlapStack.size() - 2]->append(WTFMove(m_overlapStack.last()));
    m_overlapStack.removeLast();
}

static TextStream& operator<<(TextStream& ts, const OverlapMapContainer& container)
{
    ts << container.dump(ts.indent());
    return ts;
}

TextStream& operator<<(TextStream& ts, const LayerOverlapMap& overlapMap)
{
    TextStream multilineStream;

    TextStream::GroupScope scope(ts);
    multilineStream << "LayerOverlapMap\n";
    multilineStream.increaseIndent(2);

    bool needNewline = false;
    for (auto& container : overlapMap.overlapStack()) {
        if (needNewline)
            multilineStream << "\n";
        else
            needNewline = true;
        multilineStream << indent << *container;
    }

    ts << multilineStream.release();
    return ts;
}

} // namespace WebCore

