// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_DART_EMBEDDER_DART_CONTROLLER_H_
#define MOJO_DART_EMBEDDER_DART_CONTROLLER_H_

#include "dart/runtime/include/dart_api.h"
#include "mojo/dart/embedder/isolate_data.h"
#include "mojo/public/c/system/types.h"

namespace mojo {
namespace dart {

struct DartControllerConfig {
  void* application_data;
  std::string script;
  std::string script_uri;
  std::string package_root;
  IsolateCallbacks callbacks;
  Dart_EntropySource entropy;
  const char** arguments;
  int arguments_count;
  MojoHandle handle;
  // TODO(zra): search for the --compile_all flag in arguments where needed.
  bool compile_all;
  char** error;
};

class DartController {
 public:
  static bool RunDartScript(const DartControllerConfig& config);

 private:
  static void InitVmIfNeeded(Dart_EntropySource entropy,
                             const char** arguments,
                             int arguments_count);
  static bool vmIsInitialized;
};

}  // namespace dart
}  // namespace mojo

#endif  // MOJO_DART_EMBEDDER_DART_CONTROLLER_H_
