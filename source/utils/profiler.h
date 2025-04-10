#pragma once

#include "tracy/public/tracy/Tracy.hpp"

#define CPU_EVENT(group, name) ZoneScopedN(name)
