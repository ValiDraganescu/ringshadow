// AUTOCRAFTNET_API exports the network library's classes and functions from
// its module in Unreal's modular (editor) builds, where Unreal defines it, as
// AUTOCRAFTCORE_API does for the core (AutocraftCoreApi.h). net-tests and the
// server build without Unreal, so it is empty there.
#pragma once

#if defined(AUTOCRAFTNET_API) && !defined(DLLEXPORT)
#if defined(_WIN32)
#define DLLEXPORT __declspec(dllexport)
#define DLLIMPORT __declspec(dllimport)
#else
#define DLLEXPORT __attribute__((visibility("default")))
#define DLLIMPORT __attribute__((visibility("default")))
#endif
#endif

#ifndef AUTOCRAFTNET_API
#define AUTOCRAFTNET_API
#endif
