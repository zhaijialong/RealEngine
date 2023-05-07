#pragma once

#include "microprofile/microprofile.h"

#define CPU_EVENT(group, name) MICROPROFILE_SCOPEI(group, name, MP_AUTO)
