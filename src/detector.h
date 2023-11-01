#include <string.h>
#include <stack>
#include <string>
#include <sstream>
#include <iostream>
#include <list>
#include <algorithm>

#include "dr_api.h"
#include "drmgr.h"
#include "drsyms.h"
#include "drwrap.h"
#include "dr_defines.h"

#include "heapnode.h"
#include "threadcontext.h"

#ifndef DETECTOR_H
#define DETECTOR_H

typedef enum {
    EMPTY_CALLSTACK,
    SP_NOT_FOUND,
    SUCCESS,
    FAIL
} CheckReturnResult;

#endif
