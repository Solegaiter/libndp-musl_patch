/*
 *   ndp_private.h - Neighbour discovery library private header
 *   Copyright (C) 2013 Jiri Pirko <jiri@resnulli.us>
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _NDP_PRIVATE_H_
#define _NDP_PRIVATE_H_

#include <stdarg.h>
#include <syslog.h>
#include <ndp.h>

#include "list.h"

#define NDP_EXPORT __attribute__ ((visibility("default")))

/**
 * SECTION: ndp
 * @short_description: libndp context
 */

struct ndp {
	int sock;
	void (*log_fn)(struct ndp *ndp, int priority,
		       const char *file, int line, const char *fn,
		       const char *format, va_list args);
	int log_priority;
	struct list_item msgrcv_handler_list;
};

/**
 * SECTION: logging
 * @short_description: libndp logging facility
 */

void ndp_log(struct ndp *ndp, int priority,
	     const char *file, int line, const char *fn,
	     const char *format, ...);

static inline void __attribute__((always_inline, format(printf, 2, 3)))
ndp_log_null(struct ndp *ndp, const char *format, ...) {}

#define ndp_log_cond(ndp, prio, arg...)					\
	do {								\
		if (ndp_get_log_priority(ndp) >= prio)			\
			ndp_log(ndp, prio, __FILE__, __LINE__,		\
				__FUNCTION__, ## arg);			\
	} while (0)

#ifdef ENABLE_LOGGING
#  ifdef ENABLE_DEBUG
#    define dbg(ndp, arg...) ndp_log_cond(ndp, LOG_DEBUG, ## arg)
#  else
#    define dbg(ndp, arg...) ndp_log_null(ndp, ## arg)
#  endif
#  define info(ndp, arg...) ndp_log_cond(ndp, LOG_INFO, ## arg)
#  define warn(ndp, arg...) ndp_log_cond(ndp, LOG_WARNING, ## arg)
#  define err(ndp, arg...) ndp_log_cond(ndp, LOG_ERR, ## arg)
#else
#  define dbg(ndp, arg...) ndp_log_null(ndp, ## arg)
#  define info(ndp, arg...) ndp_log_null(ndp, ## arg)
#  define warn(ndp, arg...) ndp_log_null(ndp, ## arg)
#  define err(ndp, arg...) ndp_log_null(ndp, ## arg)
#endif

/**
 * SECTION: function prototypes
 * @short_description: prototypes for internal functions
 */

#endif /* _NDP_PRIVATE_H_ */