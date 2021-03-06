/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "sky/engine/config.h"
#include "sky/engine/core/rendering/RenderLayerRepainter.h"

#include "sky/engine/core/rendering/FilterEffectRenderer.h"
#include "sky/engine/core/rendering/RenderLayer.h"
#include "sky/engine/core/rendering/RenderView.h"

namespace blink {

RenderLayerRepainter::RenderLayerRepainter(RenderLayerModelObject& renderer)
    : m_renderer(renderer)
{
}

void RenderLayerRepainter::computePaintInvalidationRectsIncludingNonCompositingDescendants()
{
    // FIXME: boundsRectForPaintInvalidation() has to walk up the parent chain
    // for every layer to compute the rects. We should make this more efficient.
    // FIXME: it's wrong to call this when layout is not up-to-date, which we do.
    m_renderer.setPreviousPaintInvalidationRect(m_renderer.boundsRectForPaintInvalidation(m_renderer.containerForPaintInvalidation()));
    // FIXME: We are only updating the paint invalidation bounds but not
    // the positionFromPaintInvalidationContainer. This means that we may
    // forcing a full invaliation of the new position. Is this really correct?

    for (RenderLayer* layer = m_renderer.layer()->firstChild(); layer; layer = layer->nextSibling()) {
        layer->paintInvalidator().computePaintInvalidationRectsIncludingNonCompositingDescendants();
    }
}

// Since we're only painting non-composited layers, we know that they all share the same paintInvalidationContainer.
void RenderLayerRepainter::paintInvalidationIncludingNonCompositingDescendants()
{
    paintInvalidationIncludingNonCompositingDescendantsInternal(m_renderer.containerForPaintInvalidation());
}

void RenderLayerRepainter::paintInvalidationIncludingNonCompositingDescendantsInternal(const RenderLayerModelObject* paintInvalidationContainer)
{
    m_renderer.invalidatePaintUsingContainer(paintInvalidationContainer, m_renderer.previousPaintInvalidationRect(), InvalidationLayer);

    for (RenderLayer* curr = m_renderer.layer()->firstChild(); curr; curr = curr->nextSibling()) {
        curr->paintInvalidator().paintInvalidationIncludingNonCompositingDescendantsInternal(paintInvalidationContainer);
    }
}

LayoutRect RenderLayerRepainter::paintInvalidationRectIncludingNonCompositingDescendants() const
{
    LayoutRect paintInvalidationRect = m_renderer.previousPaintInvalidationRect();

    for (RenderLayer* child = m_renderer.layer()->firstChild(); child; child = child->nextSibling()) {
        paintInvalidationRect.unite(child->paintInvalidator().paintInvalidationRectIncludingNonCompositingDescendants());
    }
    return paintInvalidationRect;
}

void RenderLayerRepainter::setBackingNeedsPaintInvalidationInRect(const LayoutRect& r)
{
    // FIXME(sky): Remove.
    ASSERT_NOT_REACHED();
}

void RenderLayerRepainter::setFilterBackendNeedsPaintInvalidationInRect(const LayoutRect& rect)
{
    if (rect.isEmpty())
        return;
    LayoutRect rectForPaintInvalidation = rect;

    ASSERT(m_renderer.layer()->filterInfo());

    RenderLayer* parentLayer = enclosingFilterPaintInvalidationLayer();
    ASSERT(parentLayer);
    FloatQuad paintInvalidationQuad(rectForPaintInvalidation);
    LayoutRect parentLayerRect = m_renderer.localToContainerQuad(paintInvalidationQuad, parentLayer->renderer()).enclosingBoundingBox();

    if (parentLayerRect.isEmpty())
        return;

    if (parentLayer->paintsWithFilters()) {
        parentLayer->paintInvalidator().setFilterBackendNeedsPaintInvalidationInRect(parentLayerRect);
        return;
    }

    if (parentLayer->isRootLayer()) {
        RenderView* view = toRenderView(parentLayer->renderer());
        view->invalidatePaintForRectangle(parentLayerRect);
        return;
    }

    ASSERT_NOT_REACHED();
}

RenderLayer* RenderLayerRepainter::enclosingFilterPaintInvalidationLayer() const
{
    for (const RenderLayer* curr = m_renderer.layer(); curr; curr = curr->parent()) {
        if ((curr != m_renderer.layer() && curr->requiresFullLayerImageForFilters()) || curr->isRootLayer())
            return const_cast<RenderLayer*>(curr);
    }
    return 0;
}

} // namespace blink
