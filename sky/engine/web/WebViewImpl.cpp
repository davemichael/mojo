/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "sky/engine/config.h"
#include "sky/engine/web/WebViewImpl.h"

#include "gen/sky/core/CSSValueKeywords.h"
#include "gen/sky/core/HTMLNames.h"
#include "gen/sky/platform/RuntimeEnabledFeatures.h"
#include "sky/engine/core/dom/Document.h"
#include "sky/engine/core/dom/DocumentMarkerController.h"
#include "sky/engine/core/dom/NodeRenderingTraversal.h"
#include "sky/engine/core/dom/Text.h"
#include "sky/engine/core/editing/Editor.h"
#include "sky/engine/core/editing/FrameSelection.h"
#include "sky/engine/core/editing/HTMLInterchange.h"
#include "sky/engine/core/editing/InputMethodController.h"
#include "sky/engine/core/editing/TextIterator.h"
#include "sky/engine/core/events/KeyboardEvent.h"
#include "sky/engine/core/events/WheelEvent.h"
#include "sky/engine/core/frame/FrameHost.h"
#include "sky/engine/core/frame/FrameView.h"
#include "sky/engine/core/frame/LocalFrame.h"
#include "sky/engine/core/frame/Settings.h"
#include "sky/engine/core/html/HTMLImportElement.h"
#include "sky/engine/core/html/ime/InputMethodContext.h"
#include "sky/engine/core/loader/FrameLoader.h"
#include "sky/engine/core/loader/UniqueIdentifier.h"
#include "sky/engine/core/page/Chrome.h"
#include "sky/engine/core/page/EventHandler.h"
#include "sky/engine/core/page/EventWithHitTestResults.h"
#include "sky/engine/core/page/FocusController.h"
#include "sky/engine/core/page/Page.h"
#include "sky/engine/core/page/TouchDisambiguation.h"
#include "sky/engine/core/rendering/RenderView.h"
#include "sky/engine/platform/Cursor.h"
#include "sky/engine/platform/KeyboardCodes.h"
#include "sky/engine/platform/Logging.h"
#include "sky/engine/platform/NotImplemented.h"
#include "sky/engine/platform/PlatformGestureEvent.h"
#include "sky/engine/platform/PlatformKeyboardEvent.h"
#include "sky/engine/platform/PlatformMouseEvent.h"
#include "sky/engine/platform/PlatformWheelEvent.h"
#include "sky/engine/platform/TraceEvent.h"
#include "sky/engine/platform/UserGestureIndicator.h"
#include "sky/engine/platform/exported/WebActiveGestureAnimation.h"
#include "sky/engine/platform/fonts/FontCache.h"
#include "sky/engine/platform/graphics/Color.h"
#include "sky/engine/platform/graphics/Image.h"
#include "sky/engine/platform/graphics/ImageBuffer.h"
#include "sky/engine/platform/scroll/Scrollbar.h"
#include "sky/engine/public/platform/Platform.h"
#include "sky/engine/public/platform/WebFloatPoint.h"
#include "sky/engine/public/platform/WebGestureCurve.h"
#include "sky/engine/public/platform/WebImage.h"
#include "sky/engine/public/platform/WebLayerTreeView.h"
#include "sky/engine/public/platform/WebURLRequest.h"
#include "sky/engine/public/platform/WebVector.h"
#include "sky/engine/public/web/WebActiveWheelFlingParameters.h"
#include "sky/engine/public/web/WebBeginFrameArgs.h"
#include "sky/engine/public/web/WebFrameClient.h"
#include "sky/engine/public/web/WebHitTestResult.h"
#include "sky/engine/public/web/WebNode.h"
#include "sky/engine/public/web/WebRange.h"
#include "sky/engine/public/web/WebTextInputInfo.h"
#include "sky/engine/public/web/WebViewClient.h"
#include "sky/engine/web/CompositionUnderlineVectorBuilder.h"
#include "sky/engine/web/WebInputEventConversion.h"
#include "sky/engine/web/WebLocalFrameImpl.h"
#include "sky/engine/web/WebSettingsImpl.h"
#include "sky/engine/wtf/CurrentTime.h"
#include "sky/engine/wtf/RefPtr.h"
#include "sky/engine/wtf/TemporaryChange.h"

// Get rid of WTF's pow define so we can use std::pow.
#undef pow
#include <cmath> // for std::pow

namespace blink {

// WebView ----------------------------------------------------------------

WebView* WebView::create(WebViewClient* client)
{
    // Pass the WebViewImpl's self-reference to the caller.
    return WebViewImpl::create(client);
}

WebViewImpl* WebViewImpl::create(WebViewClient* client)
{
    // Pass the WebViewImpl's self-reference to the caller.
    return adoptRef(new WebViewImpl(client)).leakRef();
}

void WebViewImpl::setMainFrame(WebFrame* frame)
{
    toWebLocalFrameImpl(frame)->initializeCoreFrame(&page()->frameHost());
}

void WebViewImpl::setSpellCheckClient(WebSpellCheckClient* spellCheckClient)
{
    m_spellCheckClient = spellCheckClient;
}

WebViewImpl::WebViewImpl(WebViewClient* client)
    : m_client(client)
    , m_spellCheckClient(0)
    , m_chromeClientImpl(this)
    , m_editorClientImpl(this)
    , m_spellCheckerClientImpl(this)
    , m_fixedLayoutSizeLock(false)
    , m_ignoreInputEvents(false)
    , m_compositorDeviceScaleFactorOverride(0)
    , m_rootLayerScale(1)
    , m_suppressNextKeypressEvent(false)
    , m_imeAcceptEvents(true)
    , m_isTransparent(false)
    , m_tabsToLinks(false)
    , m_rootLayer(0)
    , m_matchesHeuristicsForGpuRasterization(false)
    , m_recreatingGraphicsContext(false)
    , m_flingModifier(0)
    , m_flingSourceDevice(false)
    , m_showFPSCounter(false)
    , m_showPaintRects(false)
    , m_showDebugBorders(false)
    , m_continuousPaintingEnabled(false)
    , m_showScrollBottleneckRects(false)
    , m_baseBackgroundColor(Color::white)
    , m_backgroundColorOverride(Color::transparent)
    , m_userGestureObserved(false)
{
    Page::PageClients pageClients;
    pageClients.chromeClient = &m_chromeClientImpl;
    pageClients.editorClient = &m_editorClientImpl;
    pageClients.spellCheckerClient = &m_spellCheckerClientImpl;

    m_page = adoptPtr(new Page(pageClients, m_client->services()));

    setDeviceScaleFactor(m_client->screenInfo().deviceScaleFactor);
    setVisibilityState(m_client->visibilityState(), true);

    m_client->initializeLayerTreeView();
}

WebViewImpl::~WebViewImpl()
{
    ASSERT(!m_page);
}

WebLocalFrameImpl* WebViewImpl::mainFrameImpl()
{
    return m_page ? WebLocalFrameImpl::fromFrame(m_page->mainFrame()) : 0;
}

bool WebViewImpl::tabKeyCyclesThroughElements() const
{
    ASSERT(m_page);
    return m_page->tabKeyCyclesThroughElements();
}

void WebViewImpl::setTabKeyCyclesThroughElements(bool value)
{
    if (m_page)
        m_page->setTabKeyCyclesThroughElements(value);
}

void WebViewImpl::handleMouseLeave(LocalFrame& mainFrame, const WebMouseEvent& event)
{
    m_client->setMouseOverURL(WebURL());
    PageWidgetEventHandler::handleMouseLeave(mainFrame, event);
}

void WebViewImpl::handleMouseDown(LocalFrame& mainFrame, const WebMouseEvent& event)
{
    PageWidgetEventHandler::handleMouseDown(mainFrame, event);

    if (event.button == WebMouseEvent::ButtonLeft && m_mouseCaptureNode)
        m_mouseCaptureGestureToken = mainFrame.eventHandler().takeLastMouseDownGestureToken();
}

void WebViewImpl::handleMouseUp(LocalFrame& mainFrame, const WebMouseEvent& event)
{
    PageWidgetEventHandler::handleMouseUp(mainFrame, event);
}

bool WebViewImpl::handleMouseWheel(LocalFrame& mainFrame, const WebMouseWheelEvent& event)
{
    return PageWidgetEventHandler::handleMouseWheel(mainFrame, event);
}

// FIXME(sky): This appears to be unused.
bool WebViewImpl::scrollBy(const WebFloatSize& delta, const WebFloatSize& velocity)
{
    if (m_flingSourceDevice == WebGestureDeviceTouchpad) {
        WebMouseWheelEvent syntheticWheel;
        const float tickDivisor = WheelEvent::TickMultiplier;

        syntheticWheel.deltaX = delta.width;
        syntheticWheel.deltaY = delta.height;
        syntheticWheel.wheelTicksX = delta.width / tickDivisor;
        syntheticWheel.wheelTicksY = delta.height / tickDivisor;
        syntheticWheel.hasPreciseScrollingDeltas = true;
        syntheticWheel.x = m_positionOnFlingStart.x;
        syntheticWheel.y = m_positionOnFlingStart.y;
        syntheticWheel.globalX = m_globalPositionOnFlingStart.x;
        syntheticWheel.globalY = m_globalPositionOnFlingStart.y;
        syntheticWheel.modifiers = m_flingModifier;

        if (m_page && m_page->mainFrame() && m_page->mainFrame()->view())
            return handleMouseWheel(*m_page->mainFrame(), syntheticWheel);
    } else {
        WebGestureEvent syntheticGestureEvent;

        syntheticGestureEvent.type = WebInputEvent::GestureScrollUpdateWithoutPropagation;
        syntheticGestureEvent.data.scrollUpdate.deltaX = delta.width;
        syntheticGestureEvent.data.scrollUpdate.deltaY = delta.height;
        syntheticGestureEvent.x = m_positionOnFlingStart.x;
        syntheticGestureEvent.y = m_positionOnFlingStart.y;
        syntheticGestureEvent.globalX = m_globalPositionOnFlingStart.x;
        syntheticGestureEvent.globalY = m_globalPositionOnFlingStart.y;
        syntheticGestureEvent.modifiers = m_flingModifier;
        syntheticGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;

        if (m_page && m_page->mainFrame() && m_page->mainFrame()->view())
            return handleGestureEvent(syntheticGestureEvent);
    }
    return false;
}

bool WebViewImpl::handleGestureEvent(const WebGestureEvent& event)
{
    bool eventSwallowed = false;
    bool eventCancelled = false; // for disambiguation

    // Special handling for slow-path fling gestures.
    switch (event.type) {
    case WebInputEvent::GestureFlingStart: {
        if (mainFrameImpl()->frame()->eventHandler().isScrollbarHandlingGestures())
            break;
        m_client->cancelScheduledContentIntents();
        m_positionOnFlingStart = WebPoint(event.x, event.y);
        m_globalPositionOnFlingStart = WebPoint(event.globalX, event.globalY);
        m_flingModifier = event.modifiers;
        m_flingSourceDevice = event.sourceDevice;
        OwnPtr<WebGestureCurve> flingCurve = adoptPtr(Platform::current()->createFlingAnimationCurve(event.sourceDevice, WebFloatPoint(event.data.flingStart.velocityX, event.data.flingStart.velocityY), WebSize()));
        ASSERT(flingCurve);
        m_gestureAnimation = WebActiveGestureAnimation::createAtAnimationStart(flingCurve.release(), this);
        scheduleAnimation();
        eventSwallowed = true;

        m_client->didHandleGestureEvent(event, eventCancelled);
        return eventSwallowed;
    }
    case WebInputEvent::GestureFlingCancel:
        if (endActiveFlingAnimation())
            eventSwallowed = true;

        m_client->didHandleGestureEvent(event, eventCancelled);
        return eventSwallowed;
    default:
        break;
    }

    PlatformGestureEventBuilder platformEvent(mainFrameImpl()->frameView(), event);

    // FIXME: Remove redundant hit tests by pushing the call to EventHandler::targetGestureEvent
    // up to this point and pass GestureEventWithHitTestResults around.

    switch (event.type) {
    case WebInputEvent::GestureTap: {
        m_client->cancelScheduledContentIntents();
        if (detectContentOnTouch(platformEvent.position())) {
            eventSwallowed = true;
            break;
        }

        eventSwallowed = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(platformEvent);
        break;
    }
    case WebInputEvent::GestureTwoFingerTap:
    case WebInputEvent::GestureLongPress:
    case WebInputEvent::GestureLongTap: {
        if (!mainFrameImpl() || !mainFrameImpl()->frameView())
            break;

        m_client->cancelScheduledContentIntents();
        eventSwallowed = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(platformEvent);
        break;
    }
    case WebInputEvent::GestureShowPress: {
        m_client->cancelScheduledContentIntents();
        eventSwallowed = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(platformEvent);
        break;
    }
    case WebInputEvent::GestureDoubleTap:
        // GestureDoubleTap is currently only used by Android for zooming. For WebCore,
        // GestureTap with tap count = 2 is used instead. So we drop GestureDoubleTap here.
        eventSwallowed = true;
        break;
    case WebInputEvent::GestureScrollBegin:
    case WebInputEvent::GesturePinchBegin:
        m_client->cancelScheduledContentIntents();
    case WebInputEvent::GestureTapDown:
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureScrollUpdate:
    case WebInputEvent::GestureScrollUpdateWithoutPropagation:
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureTapUnconfirmed:
    case WebInputEvent::GesturePinchEnd:
    case WebInputEvent::GesturePinchUpdate:
    case WebInputEvent::GestureFlingStart: {
        eventSwallowed = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(platformEvent);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }
    m_client->didHandleGestureEvent(event, eventCancelled);
    return eventSwallowed;
}

void WebViewImpl::transferActiveWheelFlingAnimation(const WebActiveWheelFlingParameters& parameters)
{
    TRACE_EVENT0("blink", "WebViewImpl::transferActiveWheelFlingAnimation");
    ASSERT(!m_gestureAnimation);
    m_positionOnFlingStart = parameters.point;
    m_globalPositionOnFlingStart = parameters.globalPoint;
    m_flingModifier = parameters.modifiers;
    OwnPtr<WebGestureCurve> curve = adoptPtr(Platform::current()->createFlingAnimationCurve(parameters.sourceDevice, WebFloatPoint(parameters.delta), parameters.cumulativeScroll));
    ASSERT(curve);
    m_gestureAnimation = WebActiveGestureAnimation::createWithTimeOffset(curve.release(), this, parameters.startTime);
    scheduleAnimation();
}

bool WebViewImpl::endActiveFlingAnimation()
{
    if (m_gestureAnimation) {
        m_gestureAnimation.clear();
        return true;
    }
    return false;
}

void WebViewImpl::setShowFPSCounter(bool show)
{
    m_showFPSCounter = show;
}

void WebViewImpl::setShowPaintRects(bool show)
{
    m_showPaintRects = show;
}

void WebViewImpl::setShowDebugBorders(bool show)
{
    m_showDebugBorders = show;
}

void WebViewImpl::setContinuousPaintingEnabled(bool enabled)
{
    m_continuousPaintingEnabled = enabled;
    m_client->scheduleAnimation();
}

void WebViewImpl::setShowScrollBottleneckRects(bool show)
{
    m_showScrollBottleneckRects = show;
}

void WebViewImpl::acceptLanguagesChanged()
{
    if (!page())
        return;

    page()->acceptLanguagesChanged();
}

bool WebViewImpl::handleKeyEvent(const WebKeyboardEvent& event)
{
    ASSERT((event.type == WebInputEvent::RawKeyDown)
        || (event.type == WebInputEvent::KeyDown)
        || (event.type == WebInputEvent::KeyUp));

    // Halt an in-progress fling on a key event.
    endActiveFlingAnimation();

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.
    // The m_suppressNextKeypressEvent is set if the KeyDown is handled by
    // Webkit. A keyDown event is typically associated with a keyPress(char)
    // event and a keyUp event. We reset this flag here as this is a new keyDown
    // event.
    m_suppressNextKeypressEvent = false;

    RefPtr<LocalFrame> focusedFrame = focusedCoreFrame();
    if (!focusedFrame)
        return false;

    RefPtr<LocalFrame> frame = focusedFrame.get();

    PlatformKeyboardEventBuilder evt(event);

    if (frame->eventHandler().keyEvent(evt)) {
        if (WebInputEvent::RawKeyDown == event.type) {
            // Suppress the next keypress event unless the focused node is a plug-in node.
            // (Flash needs these keypress events to handle non-US keyboards.)
            m_suppressNextKeypressEvent = true;
        }
        return true;
    }

    return keyEventDefault(event);
}

bool WebViewImpl::handleCharEvent(const WebKeyboardEvent& event)
{
    ASSERT(event.type == WebInputEvent::Char);

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.  The m_suppressNextKeypressEvent is set if the KeyDown is
    // handled by Webkit. A keyDown event is typically associated with a
    // keyPress(char) event and a keyUp event. We reset this flag here as it
    // only applies to the current keyPress event.
    bool suppress = m_suppressNextKeypressEvent;
    m_suppressNextKeypressEvent = false;

    LocalFrame* frame = focusedCoreFrame();
    if (!frame)
        return suppress;

    EventHandler& handler = frame->eventHandler();

    PlatformKeyboardEventBuilder evt(event);
    if (!evt.isCharacterKey())
        return true;

    // Accesskeys are triggered by char events and can't be suppressed.
    if (handler.handleAccessKey(evt))
        return true;

    // Safari 3.1 does not pass off windows system key messages (WM_SYSCHAR) to
    // the eventHandler::keyEvent. We mimic this behavior on all platforms since
    // for now we are converting other platform's key events to windows key
    // events.
    if (evt.isSystemKey())
        return false;

    if (!suppress && !handler.keyEvent(evt))
        return keyEventDefault(event);

    return true;
}

bool WebViewImpl::keyEventDefault(const WebKeyboardEvent& event)
{
    LocalFrame* frame = focusedCoreFrame();
    if (!frame)
        return false;

    switch (event.type) {
    case WebInputEvent::Char:
        if (event.windowsKeyCode == VKEY_SPACE) {
            int keyCode = ((event.modifiers & WebInputEvent::ShiftKey) ? VKEY_PRIOR : VKEY_NEXT);
            return scrollViewWithKeyboard(keyCode, event.modifiers);
        }
        break;
    case WebInputEvent::RawKeyDown:
        if (event.modifiers == WebInputEvent::ControlKey) {
            switch (event.windowsKeyCode) {
            // Match FF behavior in the sense that Ctrl+home/end are the only Ctrl
            // key combinations which affect scrolling. Safari is buggy in the
            // sense that it scrolls the page for all Ctrl+scrolling key
            // combinations. For e.g. Ctrl+pgup/pgdn/up/down, etc.
            case VKEY_HOME:
            case VKEY_END:
                break;
            default:
                return false;
            }
        }
        if (!event.isSystemKey && !(event.modifiers & WebInputEvent::ShiftKey))
            return scrollViewWithKeyboard(event.windowsKeyCode, event.modifiers);
        break;
    default:
        break;
    }
    return false;
}

bool WebViewImpl::scrollViewWithKeyboard(int keyCode, int modifiers)
{
    ScrollDirection scrollDirection;
    ScrollGranularity scrollGranularity;
    if (!mapKeyCodeForScroll(keyCode, &scrollDirection, &scrollGranularity))
        return false;

    if (LocalFrame* frame = focusedCoreFrame())
        return frame->eventHandler().bubblingScroll(scrollDirection, scrollGranularity);
    return false;
}

bool WebViewImpl::mapKeyCodeForScroll(
    int keyCode,
    ScrollDirection* scrollDirection,
    ScrollGranularity* scrollGranularity)
{
    switch (keyCode) {
    case VKEY_LEFT:
        *scrollDirection = ScrollLeft;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_RIGHT:
        *scrollDirection = ScrollRight;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_UP:
        *scrollDirection = ScrollUp;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_DOWN:
        *scrollDirection = ScrollDown;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_HOME:
        *scrollDirection = ScrollUp;
        *scrollGranularity = ScrollByDocument;
        break;
    case VKEY_END:
        *scrollDirection = ScrollDown;
        *scrollGranularity = ScrollByDocument;
        break;
    case VKEY_PRIOR:  // page up
        *scrollDirection = ScrollUp;
        *scrollGranularity = ScrollByPage;
        break;
    case VKEY_NEXT:  // page down
        *scrollDirection = ScrollDown;
        *scrollGranularity = ScrollByPage;
        break;
    default:
        return false;
    }

    return true;
}

LocalFrame* WebViewImpl::focusedCoreFrame() const
{
    return m_page ? m_page->focusController().focusedOrMainFrame() : 0;
}

WebViewImpl* WebViewImpl::fromPage(Page* page)
{
    if (!page)
        return 0;
    return static_cast<WebViewImpl*>(page->chrome().client().webView());
}

// WebWidget ------------------------------------------------------------------

void WebViewImpl::close()
{
    if (m_page) {
        // Initiate shutdown for the entire frameset.  This will cause a lot of
        // notifications to be sent.
        m_page->willBeDestroyed();
        m_page.clear();
    }

    // Reset the delegate to prevent notifications being sent as we're being
    // deleted.
    m_client = 0;

    deref();  // Balances ref() acquired in WebView::create
}

WebSize WebViewImpl::size()
{
    return m_size;
}

WebLocalFrameImpl* WebViewImpl::localFrameRootTemporary() const
{
    // FIXME(sky): remove
    return WebLocalFrameImpl::fromFrame(page()->mainFrame());
}

void WebViewImpl::performResize()
{
    updateMainFrameLayoutSize();

    // If the virtual viewport pinch mode is enabled, the main frame will be resized
    // after layout so it can be sized to the contentsSize.
    if (localFrameRootTemporary()->frameView())
        localFrameRootTemporary()->frameView()->resize(m_size);
}

void WebViewImpl::resize(const WebSize& newSize)
{
    if (m_size == newSize)
        return;

    FrameView* view = localFrameRootTemporary()->frameView();
    if (!view)
        return;

    m_size = newSize;
    performResize();
    sendResizeEventAndRepaint();
}

void WebViewImpl::beginFrame(const WebBeginFrameArgs& frameTime)
{
    TRACE_EVENT0("blink", "WebViewImpl::beginFrame");

    WebBeginFrameArgs validFrameTime(frameTime);
    if (!validFrameTime.lastFrameTimeMonotonic)
        validFrameTime.lastFrameTimeMonotonic = monotonicallyIncreasingTime();

    // Create synthetic wheel events as necessary for fling.
    if (m_gestureAnimation) {
        if (m_gestureAnimation->animate(validFrameTime.lastFrameTimeMonotonic))
            scheduleAnimation();
        else {
            endActiveFlingAnimation();

            PlatformGestureEvent endScrollEvent(PlatformEvent::GestureScrollEnd,
                m_positionOnFlingStart, m_globalPositionOnFlingStart,
                IntSize(), 0, false, false, false, false,
                0, 0, 0, 0);

            mainFrameImpl()->frame()->eventHandler().handleGestureScrollEnd(endScrollEvent);
        }
    }

    WTF_LOG(ScriptedAnimationController, "WebViewImpl::beginFrame: page = %d", !m_page ? 0 : 1);
    if (!m_page)
        return;

    PageWidgetDelegate::animate(m_page.get(), validFrameTime.lastFrameTimeMonotonic);

    if (m_continuousPaintingEnabled)
        m_client->scheduleAnimation();
}

void WebViewImpl::layout()
{
    TRACE_EVENT0("blink", "WebViewImpl::layout");
    if (!localFrameRootTemporary())
        return;
    PageWidgetDelegate::layout(m_page.get(), localFrameRootTemporary()->frame());
}

void WebViewImpl::paint(WebCanvas* canvas, const WebRect& rect)
{
    double paintStart = currentTime();
    PageWidgetDelegate::paint(m_page.get(), canvas, rect, isTransparent() ? PageWidgetDelegate::Translucent : PageWidgetDelegate::Opaque);
    double paintEnd = currentTime();
    double pixelsPerSec = (rect.width * rect.height) / (paintEnd - paintStart);
    Platform::current()->histogramCustomCounts("Renderer4.SoftwarePaintDurationMS", (paintEnd - paintStart) * 1000, 0, 120, 30);
    Platform::current()->histogramCustomCounts("Renderer4.SoftwarePaintMegapixPerSecond", pixelsPerSec / 1000000, 10, 210, 30);
}

bool WebViewImpl::isTrackingRepaints() const
{
    if (!page())
        return false;
    FrameView* view = page()->mainFrame()->view();
    return view->isTrackingPaintInvalidations();
}

const WebInputEvent* WebViewImpl::m_currentInputEvent = 0;

// FIXME: autogenerate this kind of code, and use it throughout Blink rather than
// the one-offs for subsets of these values.
static String inputTypeToName(WebInputEvent::Type type)
{
    switch (type) {
    case WebInputEvent::MouseDown:
        return EventTypeNames::mousedown;
    case WebInputEvent::MouseUp:
        return EventTypeNames::mouseup;
    case WebInputEvent::MouseMove:
        return EventTypeNames::mousemove;
    case WebInputEvent::MouseEnter:
        return EventTypeNames::mouseenter;
    case WebInputEvent::MouseLeave:
        return EventTypeNames::mouseleave;
    case WebInputEvent::MouseWheel:
        return EventTypeNames::mousewheel;
    case WebInputEvent::KeyDown:
        return EventTypeNames::keydown;
    case WebInputEvent::KeyUp:
        return EventTypeNames::keyup;
    case WebInputEvent::GestureScrollBegin:
        return EventTypeNames::gesturescrollstart;
    case WebInputEvent::GestureScrollEnd:
        return EventTypeNames::gesturescrollend;
    case WebInputEvent::GestureScrollUpdate:
        return EventTypeNames::gesturescrollupdate;
    case WebInputEvent::GestureTapDown:
        return EventTypeNames::gesturetapdown;
    case WebInputEvent::GestureShowPress:
        return EventTypeNames::gestureshowpress;
    case WebInputEvent::GestureTap:
        return EventTypeNames::gesturetap;
    case WebInputEvent::GestureTapUnconfirmed:
        return EventTypeNames::gesturetapunconfirmed;
    case WebInputEvent::TouchStart:
        return EventTypeNames::touchstart;
    case WebInputEvent::TouchMove:
        return EventTypeNames::touchmove;
    case WebInputEvent::TouchEnd:
        return EventTypeNames::touchend;
    case WebInputEvent::TouchCancel:
        return EventTypeNames::touchcancel;
    default:
        return String("unknown");
    }
}

bool WebViewImpl::handleInputEvent(const WebInputEvent& inputEvent)
{
    TRACE_EVENT1("input", "WebViewImpl::handleInputEvent", "type", inputTypeToName(inputEvent.type).ascii().data());

    // Report the event to be NOT processed by WebKit, so that the browser can handle it appropriately.
    if (m_ignoreInputEvents)
        return false;

    TemporaryChange<const WebInputEvent*> currentEventChange(m_currentInputEvent, &inputEvent);

    if (m_mouseCaptureNode && WebInputEvent::isMouseEventType(inputEvent.type)) {
        TRACE_EVENT1("input", "captured mouse event", "type", inputEvent.type);
        // Save m_mouseCaptureNode since mouseCaptureLost() will clear it.
        RefPtr<Node> node = m_mouseCaptureNode;

        // Not all platforms call mouseCaptureLost() directly.
        if (inputEvent.type == WebInputEvent::MouseUp)
            mouseCaptureLost();

        OwnPtr<UserGestureIndicator> gestureIndicator;

        AtomicString eventType;
        switch (inputEvent.type) {
        case WebInputEvent::MouseMove:
            eventType = EventTypeNames::mousemove;
            break;
        case WebInputEvent::MouseLeave:
            eventType = EventTypeNames::mouseout;
            break;
        case WebInputEvent::MouseDown:
            eventType = EventTypeNames::mousedown;
            gestureIndicator = adoptPtr(new UserGestureIndicator(DefinitelyProcessingNewUserGesture));
            m_mouseCaptureGestureToken = gestureIndicator->currentToken();
            break;
        case WebInputEvent::MouseUp:
            eventType = EventTypeNames::mouseup;
            gestureIndicator = adoptPtr(new UserGestureIndicator(m_mouseCaptureGestureToken.release()));
            break;
        default:
            ASSERT_NOT_REACHED();
        }

        node->dispatchMouseEvent(
            PlatformMouseEventBuilder(mainFrameImpl()->frameView(), static_cast<const WebMouseEvent&>(inputEvent)),
            eventType, static_cast<const WebMouseEvent&>(inputEvent).clickCount);
        return true;
    }

    return PageWidgetDelegate::handleInputEvent(m_page.get(), *this, inputEvent);
}

void WebViewImpl::setCursorVisibilityState(bool isVisible)
{
    if (m_page)
        m_page->setIsCursorVisible(isVisible);
}

void WebViewImpl::mouseCaptureLost()
{
    TRACE_EVENT_ASYNC_END0("input", "capturing mouse", this);
    m_mouseCaptureNode = nullptr;
}

void WebViewImpl::setFocus(bool enable)
{
    m_page->focusController().setFocused(enable);
    if (enable) {
        m_page->focusController().setActive(true);
        RefPtr<LocalFrame> focusedFrame = m_page->focusController().focusedFrame();
        if (focusedFrame) {
            LocalFrame* localFrame = focusedFrame.get();
            Element* element = localFrame->document()->focusedElement();
            if (element && localFrame->selection().selection().isNone()) {
                // If the selection was cleared while the WebView was not
                // focused, then the focus element shows with a focus ring but
                // no caret and does respond to keyboard inputs.
                if (element->isContentEditable()) {
                    // updateFocusAppearance() selects all the text of
                    // contentseditable DIVs. So we set the selection explicitly
                    // instead. Note that this has the side effect of moving the
                    // caret back to the beginning of the text.
                    Position position(element, 0, Position::PositionIsOffsetInAnchor);
                    localFrame->selection().setSelection(VisibleSelection(position, SEL_DEFAULT_AFFINITY));
                }
            }
        }
        m_imeAcceptEvents = true;
    } else {
        // Clear focus on the currently focused frame if any.
        if (!m_page)
            return;

        RefPtr<LocalFrame> focusedFrame = m_page->focusController().focusedFrame();
        if (focusedFrame) {
            // Finish an ongoing composition to delete the composition node.
            if (focusedFrame->inputMethodController().hasComposition()) {
                focusedFrame->inputMethodController().confirmComposition();
            }
            m_imeAcceptEvents = false;
        }
    }
}

bool WebViewImpl::setComposition(
    const WebString& text,
    const WebVector<WebCompositionUnderline>& underlines,
    int selectionStart,
    int selectionEnd)
{
    LocalFrame* focused = focusedCoreFrame();
    if (!focused || !m_imeAcceptEvents)
        return false;

    // The input focus has been moved to another WebWidget object.
    // We should use this |editor| object only to complete the ongoing
    // composition.
    InputMethodController& inputMethodController = focused->inputMethodController();
    if (!focused->editor().canEdit() && !inputMethodController.hasComposition())
        return false;

    // We should verify the parent node of this IME composition node are
    // editable because JavaScript may delete a parent node of the composition
    // node. In this case, WebKit crashes while deleting texts from the parent
    // node, which doesn't exist any longer.
    RefPtr<Range> range = inputMethodController.compositionRange();
    if (range) {
        Node* node = range->startContainer();
        if (!node || !node->isContentEditable())
            return false;
    }

    // If we're not going to fire a keypress event, then the keydown event was
    // canceled.  In that case, cancel any existing composition.
    if (text.isEmpty() || m_suppressNextKeypressEvent) {
        // A browser process sent an IPC message which does not contain a valid
        // string, which means an ongoing composition has been canceled.
        // If the ongoing composition has been canceled, replace the ongoing
        // composition string with an empty string and complete it.
        String emptyString;
        Vector<CompositionUnderline> emptyUnderlines;
        inputMethodController.setComposition(emptyString, emptyUnderlines, 0, 0);
        return text.isEmpty();
    }

    // When the range of composition underlines overlap with the range between
    // selectionStart and selectionEnd, WebKit somehow won't paint the selection
    // at all (see InlineTextBox::paint() function in InlineTextBox.cpp).
    // But the selection range actually takes effect.
    inputMethodController.setComposition(String(text),
                           CompositionUnderlineVectorBuilder(underlines),
                           selectionStart, selectionEnd);

    return inputMethodController.hasComposition();
}

bool WebViewImpl::confirmComposition()
{
    return confirmComposition(DoNotKeepSelection);
}

bool WebViewImpl::confirmComposition(ConfirmCompositionBehavior selectionBehavior)
{
    return confirmComposition(WebString(), selectionBehavior);
}

bool WebViewImpl::confirmComposition(const WebString& text)
{
    return confirmComposition(text, DoNotKeepSelection);
}

bool WebViewImpl::confirmComposition(const WebString& text, ConfirmCompositionBehavior selectionBehavior)
{
    LocalFrame* focused = focusedCoreFrame();
    if (!focused || !m_imeAcceptEvents)
        return false;

    return focused->inputMethodController().confirmCompositionOrInsertText(text, selectionBehavior == KeepSelection ? InputMethodController::KeepSelection : InputMethodController::DoNotKeepSelection);
}

bool WebViewImpl::compositionRange(size_t* location, size_t* length)
{
    LocalFrame* focused = focusedCoreFrame();
    if (!focused || !m_imeAcceptEvents)
        return false;

    RefPtr<Range> range = focused->inputMethodController().compositionRange();
    if (!range)
        return false;

    Element* editable = focused->selection().rootEditableElementOrDocumentElement();
    ASSERT(editable);
    PlainTextRange plainTextRange(PlainTextRange::create(*editable, *range.get()));
    if (plainTextRange.isNull())
        return false;
    *location = plainTextRange.start();
    *length = plainTextRange.length();
    return true;
}

WebTextInputInfo WebViewImpl::textInputInfo()
{
    WebTextInputInfo info;

    LocalFrame* focused = focusedCoreFrame();
    if (!focused)
        return info;

    FrameSelection& selection = focused->selection();
    Element* element = selection.selection().rootEditableElement();
    if (!element)
        return info;

    info.inputMode = inputModeOfFocusedElement();

    info.type = textInputType();
    info.flags = textInputFlags();
    if (info.type == WebTextInputTypeNone)
        return info;

    if (!focused->editor().canEdit())
        return info;

    // Emits an object replacement character for each replaced element so that
    // it is exposed to IME and thus could be deleted by IME on android.
    info.value = plainText(rangeOfContents(element).get(), TextIteratorEmitsObjectReplacementCharacter);

    if (info.value.isEmpty())
        return info;

    if (RefPtr<Range> range = selection.selection().firstRange()) {
        PlainTextRange plainTextRange(PlainTextRange::create(*element, *range.get()));
        if (plainTextRange.isNotNull()) {
            info.selectionStart = plainTextRange.start();
            info.selectionEnd = plainTextRange.end();
        }
    }

    if (RefPtr<Range> range = focused->inputMethodController().compositionRange()) {
        PlainTextRange plainTextRange(PlainTextRange::create(*element, *range.get()));
        if (plainTextRange.isNotNull()) {
            info.compositionStart = plainTextRange.start();
            info.compositionEnd = plainTextRange.end();
        }
    }

    return info;
}

WebTextInputType WebViewImpl::textInputType()
{
    Element* element = focusedElement();
    if (!element)
        return WebTextInputTypeNone;

    if (element->isContentEditable(Node::UserSelectAllIsAlwaysNonEditable))
        return WebTextInputTypeContentEditable;

    return WebTextInputTypeNone;
}

int WebViewImpl::textInputFlags()
{
    Element* element = focusedElement();
    if (!element)
        return WebTextInputFlagNone;

    int flags = 0;

    const AtomicString& autocomplete = element->getAttribute("autocomplete");
    if (autocomplete == "on")
        flags |= WebTextInputFlagAutocompleteOn;
    else if (autocomplete == "off")
        flags |= WebTextInputFlagAutocompleteOff;

    const AtomicString& autocorrect = element->getAttribute("autocorrect");
    if (autocorrect == "on")
        flags |= WebTextInputFlagAutocorrectOn;
    else if (autocorrect == "off")
        flags |= WebTextInputFlagAutocorrectOff;

    const AtomicString& spellcheck = element->getAttribute("spellcheck");
    if (spellcheck == "on")
        flags |= WebTextInputFlagSpellcheckOn;
    else if (spellcheck == "off")
        flags |= WebTextInputFlagSpellcheckOff;

    return flags;
}

WebString WebViewImpl::inputModeOfFocusedElement()
{
    return WebString();
}

InputMethodContext* WebViewImpl::inputMethodContext()
{
    if (!m_imeAcceptEvents)
        return 0;

    LocalFrame* focusedFrame = focusedCoreFrame();
    if (!focusedFrame)
        return 0;

    Element* target = focusedFrame->document()->focusedElement();
    if (target && target->hasInputMethodContext())
        return &target->inputMethodContext();

    return 0;
}

void WebViewImpl::didShowCandidateWindow()
{
    if (InputMethodContext* context = inputMethodContext())
        context->dispatchCandidateWindowShowEvent();
}

void WebViewImpl::didUpdateCandidateWindow()
{
    if (InputMethodContext* context = inputMethodContext())
        context->dispatchCandidateWindowUpdateEvent();
}

void WebViewImpl::didHideCandidateWindow()
{
    if (InputMethodContext* context = inputMethodContext())
        context->dispatchCandidateWindowHideEvent();
}

WebVector<WebCompositionUnderline> WebViewImpl::compositionUnderlines() const
{
    const LocalFrame* focused = focusedCoreFrame();
    if (!focused)
        return WebVector<WebCompositionUnderline>();
    const Vector<CompositionUnderline>& underlines = focused->inputMethodController().customCompositionUnderlines();
    WebVector<WebCompositionUnderline> results(underlines.size());
    for (size_t index = 0; index < underlines.size(); ++index) {
        CompositionUnderline underline = underlines[index];
        results[index] = WebCompositionUnderline(underline.startOffset, underline.endOffset, static_cast<WebColor>(underline.color.rgb()), underline.thick, static_cast<WebColor>(underline.backgroundColor.rgb()));
    }
    return results;
}

WebColor WebViewImpl::backgroundColor() const
{
    if (isTransparent())
        return Color::transparent;
    if (!m_page)
        return m_baseBackgroundColor;
    if (!m_page->mainFrame())
        return m_baseBackgroundColor;
    FrameView* view = m_page->mainFrame()->view();
    return view->documentBackgroundColor().rgb();
}

// WebView --------------------------------------------------------------------

WebSettingsImpl* WebViewImpl::settingsImpl()
{
    if (!m_webSettings)
        m_webSettings = adoptPtr(new WebSettingsImpl(&m_page->settings()));
    ASSERT(m_webSettings);
    return m_webSettings.get();
}

WebSettings* WebViewImpl::settings()
{
    return settingsImpl();
}

WebString WebViewImpl::pageEncoding() const
{
    // FIXME(sky): remove.
    if (!m_page)
        return WebString();

    return m_page->mainFrame()->document()->encodingName();
}

void WebViewImpl::setPageEncoding(const WebString& encodingName)
{
    // FIXME(sky): remove
}

WebFrame* WebViewImpl::mainFrame()
{
    return WebFrame::fromFrame(m_page ? m_page->mainFrame() : 0);
}

WebFrame* WebViewImpl::focusedFrame()
{
    return WebFrame::fromFrame(focusedCoreFrame());
}

void WebViewImpl::injectModule(const WebString& path)
{
    RefPtr<Document> document = m_page->mainFrame()->document();
    RefPtr<HTMLImportElement> import = HTMLImportElement::create(*document);
    import->setAttribute(HTMLNames::srcAttr, path);
    if (!document->documentElement())
        return;
    document->documentElement()->appendChild(import.release());
}

void WebViewImpl::setFocusedFrame(WebFrame* frame)
{
    if (!frame) {
        // Clears the focused frame if any.
        LocalFrame* focusedFrame = focusedCoreFrame();
        if (focusedFrame)
            focusedFrame->selection().setFocused(false);
        return;
    }
    LocalFrame* coreFrame = toWebLocalFrameImpl(frame)->frame();
    coreFrame->page()->focusController().setFocusedFrame(coreFrame);
}

void WebViewImpl::setInitialFocus(bool reverse)
{
    if (!m_page)
        return;
    LocalFrame* frame = page()->focusController().focusedOrMainFrame();
    if (Document* document = frame->document())
        document->setFocusedElement(nullptr);
    page()->focusController().setInitialFocus(reverse ? FocusTypeBackward : FocusTypeForward);
}

void WebViewImpl::clearFocusedElement()
{
    RefPtr<LocalFrame> localFrame = focusedCoreFrame();
    if (!localFrame)
        return;

    RefPtr<Document> document = localFrame->document();
    if (!document)
        return;

    RefPtr<Element> oldFocusedElement = document->focusedElement();

    // Clear the focused node.
    document->setFocusedElement(nullptr);

    if (!oldFocusedElement)
        return;

    // If a text field has focus, we need to make sure the selection controller
    // knows to remove selection from it. Otherwise, the text field is still
    // processing keyboard events even though focus has been moved to the page and
    // keystrokes get eaten as a result.
    if (oldFocusedElement->isContentEditable())
        localFrame->selection().clear();
}

void WebViewImpl::advanceFocus(bool reverse)
{
    page()->focusController().advanceFocus(reverse ? FocusTypeBackward : FocusTypeForward);
}

IntPoint WebViewImpl::clampOffsetAtScale(const IntPoint& offset, float scale)
{
    FrameView* view = mainFrameImpl()->frameView();
    if (!view)
        return offset;

    return view->clampOffsetAtScale(offset, scale);
}

float WebViewImpl::deviceScaleFactor() const
{
    if (!page())
        return 1;

    return page()->deviceScaleFactor();
}

void WebViewImpl::setDeviceScaleFactor(float scaleFactor)
{
    if (!page())
        return;

    page()->setDeviceScaleFactor(scaleFactor);
}

void WebViewImpl::updateMainFrameLayoutSize()
{
    if (m_fixedLayoutSizeLock || !mainFrameImpl())
        return;

    RefPtr<FrameView> view = mainFrameImpl()->frameView();
    if (!view)
        return;

    WebSize layoutSize = m_size;

    if (page()->settings().forceZeroLayoutHeight())
        layoutSize.height = 0;

    view->setLayoutSize(layoutSize);
}

IntSize WebViewImpl::contentsSize() const
{
    RenderView* root = page()->mainFrame()->contentRenderer();
    if (!root)
        return IntSize();
    return root->documentRect().size();
}

WebHitTestResult WebViewImpl::hitTestResultAt(const WebPoint& point)
{
    return coreHitTestResultAt(point);
}

HitTestResult WebViewImpl::coreHitTestResultAt(const WebPoint& point)
{
    IntPoint scaledPoint = point;
    return hitTestResultForWindowPos(scaledPoint);
}

void WebViewImpl::spellingMarkers(WebVector<uint32_t>* markers)
{
    Vector<uint32_t> result;
    LocalFrame* frame = m_page->mainFrame();
    const DocumentMarkerVector& documentMarkers = frame->document()->markers().markers();
    for (size_t i = 0; i < documentMarkers.size(); ++i)
        result.append(documentMarkers[i]->hash());
    markers->assign(result);
}

void WebViewImpl::removeSpellingMarkersUnderWords(const WebVector<WebString>& words)
{
    Vector<String> convertedWords;
    convertedWords.append(words.data(), words.size());

    LocalFrame* frame = m_page->mainFrame();
    frame->removeSpellingMarkersUnderWords(convertedWords);
}

void WebViewImpl::sendResizeEventAndRepaint()
{
    // FIXME: This is wrong. The FrameView is responsible sending a resizeEvent
    // as part of layout. Layout is also responsible for sending invalidations
    // to the embedder. This method and all callers may be wrong. -- eseidel.
    if (localFrameRootTemporary()->frameView()) {
        // Enqueues the resize event.
        localFrameRootTemporary()->frame()->document()->enqueueResizeEvent();
    }

    WebRect damagedRect(0, 0, m_size.width, m_size.height);
    m_client->didInvalidateRect(damagedRect);
}

void WebViewImpl::setCompositorDeviceScaleFactorOverride(float deviceScaleFactor)
{
    m_compositorDeviceScaleFactorOverride = deviceScaleFactor;
}

void WebViewImpl::setIsTransparent(bool isTransparent)
{
    // Set any existing frames to be transparent.
    m_page->mainFrame()->view()->setTransparent(isTransparent);

    // Future frames check this to know whether to be transparent.
    m_isTransparent = isTransparent;
}

bool WebViewImpl::isTransparent() const
{
    return m_isTransparent;
}

// FIXME(sky): This is an android webview feature. Remove it.
void WebViewImpl::setBaseBackgroundColor(WebColor color)
{
    layout();

    if (m_baseBackgroundColor == color)
        return;

    m_baseBackgroundColor = color;

    if (m_page->mainFrame())
        m_page->mainFrame()->view()->setBaseBackgroundColor(color);
}

void WebViewImpl::setIsActive(bool active)
{
    if (page())
        page()->focusController().setActive(active);
}

bool WebViewImpl::isActive() const
{
    return page() ? page()->focusController().isActive() : false;
}

void WebViewImpl::didCommitLoad(bool isNewNavigation, bool isNavigationWithinPage)
{
    endActiveFlingAnimation();
    m_userGestureObserved = false;
    if (!isNavigationWithinPage)
        UserGestureIndicator::clearProcessedUserGestureSinceLoad();
}

void WebViewImpl::layoutUpdated(WebLocalFrameImpl* webframe)
{
    if (!m_client)
        return;
    m_client->didUpdateLayout();
}

void WebViewImpl::setIgnoreInputEvents(bool newValue)
{
    ASSERT(m_ignoreInputEvents != newValue);
    m_ignoreInputEvents = newValue;
}

void WebViewImpl::setBackgroundColorOverride(WebColor color)
{
    m_backgroundColorOverride = color;
}

Element* WebViewImpl::focusedElement() const
{
    LocalFrame* frame = m_page->focusController().focusedFrame();
    if (!frame)
        return 0;

    Document* document = frame->document();
    if (!document)
        return 0;

    return document->focusedElement();
}

HitTestResult WebViewImpl::hitTestResultForWindowPos(const IntPoint& pos)
{
    IntPoint docPoint(m_page->mainFrame()->view()->windowToContents(pos));
    HitTestResult result = m_page->mainFrame()->eventHandler().hitTestResultAtPoint(docPoint, HitTestRequest::ReadOnly | HitTestRequest::Active);
    return result;
}

void WebViewImpl::setTabsToLinks(bool enable)
{
    m_tabsToLinks = enable;
}

bool WebViewImpl::tabsToLinks() const
{
    return m_tabsToLinks;
}

void WebViewImpl::suppressInvalidations(bool enable)
{
    m_client->suppressCompositorScheduling(enable);
}

void WebViewImpl::invalidateRect(const IntRect& rect)
{
    m_client->didInvalidateRect(rect);
}

void WebViewImpl::scheduleAnimation()
{
    m_client->scheduleAnimation();
}

bool WebViewImpl::detectContentOnTouch(const WebPoint& position)
{
    HitTestResult touchHit = hitTestResultForWindowPos(position);

    if (touchHit.isContentEditable())
        return false;

    Node* node = touchHit.innerNode();
    if (!node || !node->isTextNode())
        return false;

    // Ignore when tapping on links or nodes listening to click events.
    for (; node; node = NodeRenderingTraversal::parent(node)) {
        if (node->isLink() || node->willRespondToTouchEvents() || node->willRespondToMouseClickEvents())
            return false;
    }

    WebContentDetectionResult content = m_client->detectContentAround(touchHit);
    if (!content.isValid())
        return false;

    m_client->scheduleContentIntent(content.intent());
    return true;
}

void WebViewImpl::setVisibilityState(WebPageVisibilityState visibilityState,
                                     bool isInitialState) {
    if (!page())
        return;

    ASSERT(visibilityState == WebPageVisibilityStateVisible || visibilityState == WebPageVisibilityStateHidden);
    m_page->setVisibilityState(static_cast<PageVisibilityState>(static_cast<int>(visibilityState)), isInitialState);
}

} // namespace blink
