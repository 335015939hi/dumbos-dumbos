#include <errno.h>

#include "common.h"

void _log_(const char *s) { LOG("%s", s); }
void _log_verbose_(const char *s) { LOG_VERBOSE("%s", s); }
void _log_debug_(const char *s) { LOG_DEBUG("%s", s); }
void _log_warn_(const char *s) { LOG_WARN("%s", s); }
void _log_error_(const char *s) { LOG_ERR("%s", s); }
void _log_fatal_(const char *s) { LOG_FATAL("%s", s); }
void _log_errno_(const char *s) { LOG_ERRNO(s, errno); }
void _log_fatal_errno_(const char *s) { LOG_FATAL_ERRNO(s, errno); }
void _log_warn_errno_(const char *s) { LOG_WARN_ERRNO(s, errno); }
