// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "examples/wm_flow/wm/frame_controller.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/services/view_manager/public/cpp/view.h"
#include "mojo/views/native_widget_mojo.h"
#include "services/window_manager/capture_controller.h"
#include "services/window_manager/window_manager_app.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_client.h"

class FrameController::LayoutManager : public views::LayoutManager,
                                       public views::ButtonListener {
 public:
  explicit LayoutManager(FrameController* controller)
      : controller_(controller),
        capture_checkbox_(
            new views::Checkbox(base::ASCIIToUTF16("Capture"))),
        close_button_(
            new views::LabelButton(this, base::ASCIIToUTF16("Begone"))),
        maximize_button_(
            new views::LabelButton(this, base::ASCIIToUTF16("Embiggen"))) {
    capture_checkbox_->set_listener(this);
  }
  virtual ~LayoutManager() {}

 private:
  static const int kButtonFrameMargin = 5;
  static const int kButtonFrameSpacing = 2;
  static const int kFrameSize = 10;

  // Overridden from views::LayoutManager:
  virtual void Installed(views::View* host) override {
    host->AddChildView(capture_checkbox_);
    host->AddChildView(close_button_);
    host->AddChildView(maximize_button_);
  }
  virtual void Layout(views::View* host) override {
    gfx::Size ps = capture_checkbox_->GetPreferredSize();
    capture_checkbox_->SetBounds(kButtonFrameMargin, kButtonFrameMargin,
                                 ps.width(), ps.height());

    ps = close_button_->GetPreferredSize();
    gfx::Rect bounds = host->GetLocalBounds();
    close_button_->SetBounds(bounds.right() - kButtonFrameMargin - ps.width(),
                             kButtonFrameMargin, ps.width(), ps.height());

    ps = maximize_button_->GetPreferredSize();
    maximize_button_->SetBounds(
        close_button_->x() - kButtonFrameSpacing - ps.width(),
        kButtonFrameMargin, ps.width(), ps.height());

    bounds.Inset(kFrameSize,
                 close_button_->bounds().bottom() + kButtonFrameMargin,
                 kFrameSize, kFrameSize);
    controller_->app_view_->SetBounds(*mojo::Rect::From(bounds));
  }
  virtual gfx::Size GetPreferredSize(const views::View* host) const override {
    return gfx::Size();
  }

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) override {
    if (sender == close_button_)
      controller_->CloseWindow();
    else if (sender == maximize_button_)
      controller_->ToggleMaximize();
    else if (sender == capture_checkbox_)
      controller_->SetCapture(capture_checkbox_->checked());
  }

  FrameController* controller_;
  views::Checkbox* capture_checkbox_;
  views::Button* close_button_;
  views::Button* maximize_button_;

  DISALLOW_COPY_AND_ASSIGN(LayoutManager);
};

class FrameController::FrameEventHandler : public ui::EventHandler {
 public:
  explicit FrameEventHandler(FrameController* frame_controller)
      : frame_controller_(frame_controller) {}
  virtual ~FrameEventHandler() {}

 private:

  // Overriden from ui::EventHandler:
  virtual void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::ET_MOUSE_PRESSED)
      frame_controller_->ActivateWindow();
  }

  FrameController* frame_controller_;

  DISALLOW_COPY_AND_ASSIGN(FrameEventHandler);
};

////////////////////////////////////////////////////////////////////////////////
// FrameController, public:

FrameController::FrameController(
    mojo::Shell* shell,
    mojo::View* view,
    mojo::View** app_view,
    window_manager::WindowManagerApp* window_manager_app)
    : view_(view),
      app_view_(mojo::View::Create(view->view_manager())),
      frame_view_(new views::View),
      frame_view_layout_manager_(new LayoutManager(this)),
      widget_(new views::Widget),
      maximized_(false),
      window_manager_app_(window_manager_app) {
  view_->AddChild(app_view_);
  app_view_->SetVisible(true);
  view_->AddObserver(this);
  *app_view = app_view_;
  frame_view_->set_background(
      views::Background::CreateSolidBackground(SK_ColorBLUE));
  frame_view_->SetLayoutManager(frame_view_layout_manager_);
  frame_event_handler_.reset(new FrameEventHandler(this));
  frame_view_->AddPreTargetHandler(frame_event_handler_.get());
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.native_widget = new mojo::NativeWidgetMojo(widget_, shell, view_);
  params.bounds = view_->bounds().To<gfx::Rect>();
  widget_->Init(params);
  widget_->SetContentsView(frame_view_);
  widget_->Show();
}

FrameController::~FrameController() {}

void FrameController::CloseWindow() {
  // This destroys |app_view_| as it is a child of |view_|.
  view_->Destroy();
}

void FrameController::ToggleMaximize() {
  if (!maximized_)
    restored_bounds_ = view_->bounds().To<gfx::Rect>();
  maximized_ = !maximized_;
  if (maximized_)
    view_->SetBounds(view_->parent()->bounds());
  else
    view_->SetBounds(*mojo::Rect::From(restored_bounds_));
}

void FrameController::ActivateWindow() {
  window_manager_app_->focus_controller()->ActivateView(view_);
}

void FrameController::SetCapture(bool frame_has_capture) {
  if (frame_has_capture)
    window_manager_app_->capture_controller()->SetCapture(view_);
  else
    window_manager_app_->capture_controller()->ReleaseCapture(view_);
}

////////////////////////////////////////////////////////////////////////////////
// FrameController, mojo::ViewObserver implementation:

void FrameController::OnViewDestroyed(mojo::View* view) {
  view_->RemoveObserver(this);
  delete this;
}
