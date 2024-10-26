// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_SOMMELIER_UTIL_H_
#define VM_TOOLS_SOMMELIER_SOMMELIER_UTIL_H_

#include <assert.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>

#include <wayland-client-protocol.h>
#include <wayland-server.h>

#define errno_assert(rv)                                          \
  {                                                               \
    int macro_private_assert_value = (rv);                        \
    if (!macro_private_assert_value) {                            \
      fprintf(stderr, "Unexpected error: %s\n", strerror(errno)); \
      assert(false);                                              \
    }                                                             \
  }

#define UNUSED(x) ((void)(x))

// Performs an asprintf operation and checks the result for validity and calls
// abort() if there's a failure. Returns a newly allocated string rather than
// taking a double pointer argument like asprintf.
__attribute__((__format__(__printf__, 1, 0))) char* sl_xasprintf(
    const char* fmt, ...);

#define DEFAULT_DELETER_FDECL(TypeName) \
  namespace std {                       \
  template <>                           \
  struct default_delete<TypeName> {     \
    void operator()(TypeName* ptr);     \
  };                                    \
  }

DEFAULT_DELETER_FDECL(struct wl_event_source);

// Maps wl_ to sl_ types, e.g. WlToSl<wl_seat>::type == sl_host_seat.
template <typename WaylandType>
struct WlToSl;

#define MAP_STRUCTS(WlType, SlType) \
  template <>                       \
  struct WlToSl<WlType> {           \
    using type = SlType;            \
  };

// Convert a request argument of type InArg to type OutArg. InArg is the type
// sommelier receives as a Wayland host. OutArg is the type used passed to the
// real host compositor. For Wayland resources, these will be wl_resource* and
// wl_..* (e.g. wl_surface*) respectively.
template <typename InArg, typename OutArg>
struct ConvertRequestArg {};

template <>
struct ConvertRequestArg<const char*, const char*> {
  inline static const char* Convert(const char* arg) { return arg; }
};
template <>
struct ConvertRequestArg<uint32_t, uint32_t> {
  inline static uint32_t Convert(uint32_t arg) { return arg; }
};
template <>
struct ConvertRequestArg<int32_t, int32_t> {
  inline static int32_t Convert(int32_t arg) { return arg; }
};

template <typename OutArg>
struct ConvertRequestArg<wl_resource*, OutArg*> {
  static OutArg* Convert(wl_resource* resource) {
    if (!resource)
      return nullptr;
    using SlType = typename WlToSl<OutArg>::type;
    SlType* host = static_cast<SlType*>(wl_resource_get_user_data(resource));
    return host ? host->proxy : nullptr;
  }
};

template <typename T>
inline bool IsNullWlResource(T arg) {
  return false;
}
template <>
inline bool IsNullWlResource(wl_resource* resource) {
  return resource == nullptr;
}

enum class AllowNullResource {
  kNo = 0,
  kYes = 1,
};

// Invoke the given wl_ function with each arg converted. This helper struct is
// so we can extract types from the wl_ function into a parameter pack for the
// fold expression and not require them to be explicitly written out.
template <typename... T>
struct ForwardRequestHelper;
template <typename... OutArgs>
struct ForwardRequestHelper<void (*)(OutArgs...)> {
  template <auto wl_function, AllowNullResource allow_null, typename... InArgs>
  static void Forward(struct wl_client* client, InArgs... args) {
    if (allow_null == AllowNullResource::kNo) {
      if ((IsNullWlResource<InArgs>(args) || ...)) {
        fprintf(stderr, "error: received unexpected null resource in: %s\n",
                __PRETTY_FUNCTION__);
        return;
      }
    }
    wl_function(ConvertRequestArg<InArgs, OutArgs>::Convert(args)...);
  }
};

// Similar to the above template, however, it takes an extra class type so we
// can access member functions.
template <typename C, typename... OutArgs>
struct ForwardRequestHelper<void (C::*)(OutArgs...)> {
  template <auto shim_getter,
            auto function,
            AllowNullResource allow_null,
            typename... InArgs>
  static void Forward(struct wl_client* client, InArgs... args) {
    if (allow_null == AllowNullResource::kNo) {
      if ((IsNullWlResource<InArgs>(args) || ...)) {
        fprintf(stderr, "error: received unexpected null resource in: %s\n",
                __PRETTY_FUNCTION__);
        return;
      }
    }
    (shim_getter()->*function)(
        ConvertRequestArg<InArgs, OutArgs>::Convert(args)...);
  }
};

// Wraps a function which forwards a request from a client which is connected to
// Sommelier to the server Sommelier is connected to.
//
// If null Wayland resources should be allowed, AllowNullResource::kYes should
// be set, otherwise the request will be considered invalid and dropped.
// Example usage:
// - ForwardRequest<wl_shell_surface_move>,
// - ForwardRequest<wl_shell_surface_set_fullscreen, AllowNullResource::kYes>
//
// The first argument (receiving object) is guaranteed by Wayland to be
// non-null but for code simplicity it is handled the same as the request
// arguments, with null being allowed or disallowed based on |allow_null|.
template <auto wl_function,
          AllowNullResource allow_null = AllowNullResource::kNo,
          typename... InArgs>
void ForwardRequest(InArgs... args) {
  ForwardRequestHelper<decltype(wl_function)>::template Forward<wl_function,
                                                                allow_null>(
      args...);
}

// Same as the above function, but it wraps around the shim functions that are
// generated via gen-shim.py.
// Usage:
//  - ForwardRequestShim<xdg_positioner_shim, set_anchor>
//  - ForwardRequestShim<xdg_positioner_shim, set_anchor,
//                       AllowNullResource::kYes>
template <auto shim_getter,
          auto function,
          AllowNullResource allow_null = AllowNullResource::kNo,
          typename... InArgs>
void ForwardRequestToShim(InArgs... args) {
  ForwardRequestHelper<decltype(function)>::template Forward<
      shim_getter, function, allow_null>(args...);
}

// Wraps a function which forwards an event from a server which Sommelier is
// connected to to a client which is connected to Sommelier.
//
// Example usage:
//  - ForwardEvent<wl_shell_surface_send_ping>
template <auto wl_function, typename T, typename... InArgs>
void ForwardEvent(void* data, T* resource, InArgs... args) {
  using SlType = typename WlToSl<T>::type;

  SlType* host = static_cast<SlType*>(
      wl_proxy_get_user_data(reinterpret_cast<struct wl_proxy*>(resource)));
  wl_function(host->resource, args...);
}

// Utility templated function to help readability of unimplemented/ignored
// implementations of listener/implementation functions.
// Example usage:
//   static const struct zaura_output_listener sl_aura_output_listener = {
//     DoNothing, DoNothing, DoNothing,
//     DoNothing, DoNothing
//   }
template <typename... InArgs>
void DoNothing(InArgs... ignored) {}

#endif  // VM_TOOLS_SOMMELIER_SOMMELIER_UTIL_H_
