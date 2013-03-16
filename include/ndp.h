/*
 *   ndp.h - Neighbour discovery library
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

#ifndef _NDP_H_
#define _NDP_H_

#include <stdbool.h>
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ndp;

void ndp_set_log_fn(struct ndp *ndp,
		    void (*log_fn)(struct ndp *ndp, int priority,
				   const char *file, int line, const char *fn,
				   const char *format, va_list args));
int ndp_get_log_priority(struct ndp *ndp);
void ndp_set_log_priority(struct ndp *ndp, int priority);

struct ndp_msg;
struct ndp_msgrs;
struct ndp_msgra;
struct ndp_msgns;
struct ndp_msgna;
struct ndp_msgr;

enum ndp_msg_type {
	NDP_MSG_RS, /* Router Solicitation */
	NDP_MSG_RA, /* Router Advertisement */
	NDP_MSG_NS, /* Neighbor Solicitation */
	NDP_MSG_NA, /* Neighbor Advertisement */
	NDP_MSG_R, /* Redirect */
	NDP_MSG_ALL, /* Matches all */
};

uint8_t ndp_msgra_curhoplimit(struct ndp_msgra *msgra);
void ndp_msgra_curhoplimit_set(struct ndp_msgra *msgra, uint8_t curhoplimit);
bool ndp_msgra_flag_managed(struct ndp_msgra *msgra);
void ndp_msgra_flag_managed_set(struct ndp_msgra *msgra, bool flag_managed);
bool ndp_msgra_flag_other(struct ndp_msgra *msgra);
void ndp_msgra_flag_other_set(struct ndp_msgra *msgra, bool flag_other);
bool ndp_msgra_flag_home_agent(struct ndp_msgra *msgra);
void ndp_msgra_flag_home_agent_set(struct ndp_msgra *msgra,
				   bool flag_home_agent);
uint16_t ndp_msgra_router_lifetime(struct ndp_msgra *msgra);
void ndp_msgra_router_lifetime_set(struct ndp_msgra *msgra,
				   uint16_t router_lifetime);
uint32_t ndp_msgra_reachable_time(struct ndp_msgra *msgra);
void ndp_msgra_reachable_time_set(struct ndp_msgra *msgra,
				  uint32_t reachable_time);
uint32_t ndp_msgra_retransmit_time(struct ndp_msgra *msgra);
void ndp_msgra_retransmit_time_set(struct ndp_msgra *msgra,
				   uint32_t retransmit_time);

bool ndp_msgra_opt_source_linkaddr_present(struct ndp_msgra *msgra);
unsigned char *ndp_msgra_opt_source_linkaddr(struct ndp_msgra *msgra);
size_t ndp_msgra_opt_source_linkaddr_len(struct ndp_msgra *msgra);
bool ndp_msgra_opt_target_linkaddr_present(struct ndp_msgra *msgra);
unsigned char *ndp_msgra_opt_target_linkaddr(struct ndp_msgra *msgra);
size_t ndp_msgra_opt_target_linkaddr_len(struct ndp_msgra *msgra);

bool ndp_msgra_opt_prefix_present(struct ndp_msgra *msgra);
struct in6_addr *ndp_msgra_opt_prefix(struct ndp_msgra *msgra);
uint8_t ndp_msgra_opt_prefix_len(struct ndp_msgra *msgra);
uint32_t ndp_msgra_opt_prefix_valid_time(struct ndp_msgra *msgra);
uint32_t ndp_msgra_opt_prefix_preferred_time(struct ndp_msgra *msgra);
bool ndp_msgra_opt_mtu_present(struct ndp_msgra *msgra);
uint32_t ndp_msgra_opt_mtu(struct ndp_msgra *msgra);

int ndp_msg_new(struct ndp_msg **p_msg, enum ndp_msg_type msg_type);
void ndp_msg_destroy(struct ndp_msg *msg);
void *ndp_msg_payload(struct ndp_msg *msg);
size_t ndp_msg_payload_len(struct ndp_msg *msg);
void ndp_msg_payload_len_set(struct ndp_msg *msg, size_t len);
void *ndp_msg_payload_opts(struct ndp_msg *msg);
size_t ndp_msg_payload_opts_len(struct ndp_msg *msg);
struct ndp_msgrs *ndp_msgrs(struct ndp_msg *msg);
struct ndp_msgra *ndp_msgra(struct ndp_msg *msg);
struct ndp_msgns *ndp_msgns(struct ndp_msg *msg);
struct ndp_msgna *ndp_msgna(struct ndp_msg *msg);
struct ndp_msgr *ndp_msgr(struct ndp_msg *msg);
enum ndp_msg_type ndp_msg_type(struct ndp_msg *msg);
struct in6_addr *ndp_msg_addrto(struct ndp_msg *msg);
uint32_t ndp_msg_ifindex(struct ndp_msg *msg);

typedef int (*ndp_msgrcv_handler_func_t)(struct ndp *ndp, struct ndp_msg *msg,
					 void *priv);
int ndp_msgrcv_handler_register(struct ndp *ndp, ndp_msgrcv_handler_func_t func,
				enum ndp_msg_type msg_type, uint32_t ifindex,
				void *priv);
void ndp_msgrcv_handler_unregister(struct ndp *ndp, ndp_msgrcv_handler_func_t func,
				   enum ndp_msg_type msg_type, uint32_t ifindex,
				   void *priv);

struct ndp_eventfd;

struct ndp_eventfd *ndp_get_next_eventfd(struct ndp *ndp,
					 struct ndp_eventfd *eventfd);
#define ndp_for_each_event_fd(eventfd, ndp)				\
	for (eventfd = ndp_get_next_eventfd(ndp, NULL); eventfd;	\
	     eventfd = ndp_get_next_eventfd(ndp, eventfd))
int ndp_get_eventfd_fd(struct ndp *ndp, struct ndp_eventfd *eventfd);
int ndp_call_eventfd_handler(struct ndp *ndp, struct ndp_eventfd *eventfd);

int ndp_open(struct ndp **p_ndp);
void ndp_close(struct ndp *ndp);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _NDP_H_ */