#pragma once

#if defined(TRACY_ENABLE) || defined(ENGINE_PROFILING_ENABLE)
#include <tracy/Tracy.hpp>

#define ENGINE_PROFILE_ZONE() ZoneScoped
#define ENGINE_PROFILE_ZONE_NAMED(name) ZoneScopedN(name)
#define ENGINE_PROFILE_FRAME() FrameMark
#define ENGINE_PROFILE_FRAME_NAMED(name) FrameMarkNamed(name)
#else
#define ENGINE_PROFILE_ZONE()
#define ENGINE_PROFILE_ZONE_NAMED(name)
#define ENGINE_PROFILE_FRAME()
#define ENGINE_PROFILE_FRAME_NAMED(name)
#endif
