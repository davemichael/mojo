// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/surfaces/surfaces_service_impl.h"

namespace surfaces {

SurfacesServiceImpl::SurfacesServiceImpl(
    cc::SurfaceManager* manager,
    uint32_t* next_id_namespace,
    SurfacesImpl::Client* client,
    mojo::InterfaceRequest<mojo::SurfacesService> request)
    : manager_(manager),
      next_id_namespace_(next_id_namespace),
      client_(client),
      binding_(this, request.Pass()) {
}

SurfacesServiceImpl::~SurfacesServiceImpl() {
}

void SurfacesServiceImpl::CreateSurfaceConnection(
    const mojo::Callback<void(mojo::SurfacePtr, uint32_t)>& callback) {
  uint32_t id_namespace = (*next_id_namespace_)++;
  mojo::SurfacePtr surface;
  new SurfacesImpl(manager_, id_namespace, client_, &surface);
  callback.Run(surface.Pass(), id_namespace);
}

}  // namespace surfaces
