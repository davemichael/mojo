// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHELL_DYNAMIC_SERVICE_RUNNER_H_
#define SHELL_DYNAMIC_SERVICE_RUNNER_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/native_library.h"
#include "mojo/public/cpp/system/core.h"

namespace base {
class FilePath;
}

namespace mojo {
namespace shell {

class Context;

// Abstraction for loading a service (from the file system) and running it (on
// another thread or in a separate process).
class DynamicServiceRunner {
 public:
  virtual ~DynamicServiceRunner() {}

  // Takes ownership of the file at |app_path|. Loads the app in that file and
  // runs it on some other thread/process. |app_completed_callback| is posted
  // (to the thread on which |Start()| was called) after |MojoMain()| completes.
  virtual void Start(const base::FilePath& app_path,
                     ScopedMessagePipeHandle service_handle,
                     const base::Closure& app_completed_callback) = 0;

  // Loads the service in the DSO specificed by |app_path| and prepares it for
  // execution. Runs the DSO's exported function MojoMain().
  // The NativeLibrary is returned and ownership transferred to the caller.
  // This is so if it is unloaded at all, this can be done safely after this
  // thread is destroyed and any thread-local destructors have been executed.
  static base::NativeLibrary LoadAndRunService(
      const base::FilePath& app_path,
      ScopedMessagePipeHandle service_handle);
};

class DynamicServiceRunnerFactory {
 public:
  virtual ~DynamicServiceRunnerFactory() {}
  virtual scoped_ptr<DynamicServiceRunner> Create(Context* context) = 0;
};

// A generic factory.
template <class DynamicServiceRunnerImpl>
class DynamicServiceRunnerFactoryImpl : public DynamicServiceRunnerFactory {
 public:
  DynamicServiceRunnerFactoryImpl() {}
  virtual ~DynamicServiceRunnerFactoryImpl() {}
  virtual scoped_ptr<DynamicServiceRunner> Create(Context* context) override {
    return scoped_ptr<DynamicServiceRunner>(
        new DynamicServiceRunnerImpl(context));
  }
};

}  // namespace shell
}  // namespace mojo

#endif  // SHELL_DYNAMIC_SERVICE_RUNNER_H_
