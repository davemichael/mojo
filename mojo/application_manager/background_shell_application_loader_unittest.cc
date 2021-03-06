// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application_manager/background_shell_application_loader.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

namespace {

class DummyLoader : public ApplicationLoader {
 public:
  DummyLoader() : simulate_app_quit_(true) {}
  ~DummyLoader() override {}

  // ApplicationLoader overrides:
  void Load(ApplicationManager* manager,
            const GURL& url,
            ScopedMessagePipeHandle shell_handle,
            LoadCallback callback) override {
    if (simulate_app_quit_)
      base::MessageLoop::current()->Quit();
  }

  void OnApplicationError(ApplicationManager* manager,
                          const GURL& url) override {}

  void DontSimulateAppQuit() { simulate_app_quit_ = false; }

 private:
  bool simulate_app_quit_;
};

}  // namespace

// Tests that the loader can start and stop gracefully.
TEST(BackgroundShellApplicationLoaderTest, StartStop) {
  scoped_ptr<ApplicationLoader> real_loader(new DummyLoader());
  BackgroundShellApplicationLoader loader(
      real_loader.Pass(), "test", base::MessageLoop::TYPE_DEFAULT);
}

// Tests that the loader can load a service that is well behaved (quits
// itself).
TEST(BackgroundShellApplicationLoaderTest, Load) {
  scoped_ptr<ApplicationLoader> real_loader(new DummyLoader());
  BackgroundShellApplicationLoader loader(
      real_loader.Pass(), "test", base::MessageLoop::TYPE_DEFAULT);
  MessagePipe dummy;
  loader.Load(NULL, GURL(), dummy.handle0.Pass(),
              ApplicationLoader::SimpleLoadCallback());
}

}  // namespace mojo
