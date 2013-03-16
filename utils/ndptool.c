/*
 *   ndptool.c - Neighbour discovery tool
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <getopt.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ndp.h>

enum verbosity_level {
	VERB1,
	VERB2,
	VERB3,
	VERB4,
};

#define DEFAULT_VERB VERB1
static int g_verbosity = DEFAULT_VERB;

#define pr_err(args...) fprintf(stderr, ##args)
#define pr_outx(verb_level, args...)			\
	do {						\
		if (verb_level <= g_verbosity)		\
			fprintf(stdout, ##args);	\
	} while (0)
#define pr_out(args...) pr_outx(DEFAULT_VERB, ##args)
#define pr_out2(args...) pr_outx(VERB2, ##args)
#define pr_out3(args...) pr_outx(VERB3, ##args)
#define pr_out4(args...) pr_outx(VERB4, ##args)

static int run_main_loop(struct ndp *ndp)
{
	fd_set rfds;
	fd_set rfds_tmp;
	int fdmax;
	int ret;
	struct ndp_eventfd *eventfd;
	sigset_t mask;
	int sfd;
	int err = 0;

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGQUIT);

	ret = sigprocmask(SIG_BLOCK, &mask, NULL);
	if (ret == -1) {
		pr_err("Failed to set blocked signals\n");
		return -errno;
	}

	sfd = signalfd(-1, &mask, 0);
	if (sfd == -1) {
		pr_err("Failed to open signalfd\n");
		return -errno;
	}

	FD_ZERO(&rfds);
	FD_SET(sfd, &rfds);
	fdmax = sfd;

	ndp_for_each_event_fd(eventfd, ndp) {
		int fd = ndp_get_eventfd_fd(ndp, eventfd);

		FD_SET(fd, &rfds);
		if (fd > fdmax)
			fdmax = fd;
	}
	fdmax++;

	for (;;) {
		rfds_tmp = rfds;
		ret = select(fdmax, &rfds_tmp, NULL, NULL, NULL);
		if (ret == -1) {
			pr_err("Select failed\n");
			err = -errno;
			goto out;
		}
		if (FD_ISSET(sfd, &rfds_tmp)) {
			struct signalfd_siginfo fdsi;
			ssize_t len;

			len = read(sfd, &fdsi, sizeof(struct signalfd_siginfo));
		        if (len != sizeof(struct signalfd_siginfo)) {
				pr_err("Unexpected data length came from signalfd\n");
				err = -EINVAL;
				goto out;
			}
			switch (fdsi.ssi_signo) {
			case SIGINT:
			case SIGQUIT:
			case SIGTERM:
				goto out;
			default:
				pr_err("Read unexpected signal\n");
				err = -EINVAL;
				goto out;
			}

		}
		ndp_for_each_event_fd(eventfd, ndp) {
			if (FD_ISSET(ndp_get_eventfd_fd(ndp, eventfd), &rfds_tmp))
				err = ndp_call_eventfd_handler(ndp, eventfd);
				if (err) {
					pr_err("ndp eventfd handler call failed\n");
					return err;
				}
		}
	}
out:
	close(sfd);
	return err;
}

static void print_help(const char *argv0) {
	pr_out(
            "%s [options] command\n"
            "\t-h --help                Show this help\n"
            "\t-v --verbose             Increase output verbosity\n"
            "\t-t --msg_type=TYPE       Specified message type\n"
            "\t-i --ifname=IFNAME       Specified interface name\n"
	    "\t                         (\"rs\", \"ra\", \"ns\", \"na\")\n"
	    "Available commands:\n"
	    "\tmonitor\n",
            argv0);
}

static const char *str_in6_addr(struct in6_addr *addr)
{
	static char buf[INET6_ADDRSTRLEN];

	return inet_ntop(AF_INET6, addr, buf, sizeof(buf));
}

static void pr_out_hwaddr(unsigned char *hwaddr, size_t len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (i)
			pr_out(":");
		pr_out("%02x", hwaddr[i]);
	}
	pr_out("\n");
}

static int msgrcv_handler_func(struct ndp *ndp, struct ndp_msg *msg, void *priv)
{
	char ifname[IF_NAMESIZE];
	enum ndp_msg_type msg_type = ndp_msg_type(msg);

	if_indextoname(ndp_msg_ifindex(msg), ifname);
	pr_out("NDP payload len %lu, from addr: %s, iface: %s\n",
	       ndp_msg_payload_len(msg),
	       str_in6_addr(ndp_msg_addrto(msg)), ifname);
	if (msg_type == NDP_MSG_RS) {
		pr_out("  Type: RS\n");
	} else if (msg_type == NDP_MSG_RA) {
		struct ndp_msgra *msgra = ndp_msgra(msg);

		pr_out("  Type: RA\n");
		pr_out("  Hop limit: %u\n", ndp_msgra_curhoplimit(msgra));
		pr_out("  Managed address configuration: %s\n",
		       ndp_msgra_flag_managed(msgra) ? "yes" : "no");
		pr_out("  Other configuration: %s\n",
		       ndp_msgra_flag_other(msgra) ? "yes" : "no");
		pr_out("  Router lifetime: %us\n",
		       ndp_msgra_router_lifetime(msgra));
		pr_out("  Reachable time: ");
		if (ndp_msgra_reachable_time(msgra)) {
			pr_out("%ums\n",
			       ndp_msgra_reachable_time(msgra));
		} else {
			pr_out("unspecified\n");
		}
		pr_out("  Retransmit time: ");
		if (ndp_msgra_retransmit_time(msgra)) {
			pr_out("%ums\n",
			       ndp_msgra_retransmit_time(msgra));
		} else {
			pr_out("unspecified\n");
		}
		if (ndp_msgra_opt_source_linkaddr_present(msgra)) {
			pr_out("  Source linkaddr: ");
			pr_out_hwaddr(ndp_msgra_opt_source_linkaddr(msgra),
				      ndp_msgra_opt_source_linkaddr_len(msgra));
		}
		if (ndp_msgra_opt_target_linkaddr_present(msgra)) {
			pr_out("  Target linkaddr: ");
			pr_out_hwaddr(ndp_msgra_opt_target_linkaddr(msgra),
				      ndp_msgra_opt_target_linkaddr_len(msgra));
		}
		if (ndp_msgra_opt_prefix_present(msgra)) {
			uint32_t valid_time;
			uint32_t preferred_time;

			valid_time = ndp_msgra_opt_prefix_valid_time(msgra);
			preferred_time = ndp_msgra_opt_prefix_preferred_time(msgra);
			pr_out("  Prefix: %s/%u",
			       str_in6_addr(ndp_msgra_opt_prefix(msgra)),
			       ndp_msgra_opt_prefix_len(msgra));
			pr_out(", valid_time: ");
			if (valid_time == (uint32_t) -1)
				pr_out("infinity");
			else
				pr_out("%us", valid_time);
			pr_out(", preferred_time: ");
			if (preferred_time == (uint32_t) -1)
				pr_out("infinity");
			else
				pr_out("%us", preferred_time);
			pr_out("\n");
		}
		if (ndp_msgra_opt_mtu_present(msgra))
			pr_out("  MTU: %u\n", ndp_msgra_opt_mtu(msgra));
	} else if (msg_type == NDP_MSG_NS) {
		pr_out("  Type: NS\n");
	} else if (msg_type == NDP_MSG_NA) {
		pr_out("  Type: NA\n");
	} else if (msg_type == NDP_MSG_R) {
		pr_out("  Type: R\n");
	} else {
		return 0;
	}
	return 0;
}

static int run_cmd_monitor(struct ndp *ndp, enum ndp_msg_type msg_type,
			   uint32_t ifindex)
{
	int err;

	err = ndp_msgrcv_handler_register(ndp, &msgrcv_handler_func, msg_type,
					  ifindex, NULL);
	if (err) {
		pr_err("Failed to register msgrcv handler\n");
		return err;
	}
	err = run_main_loop(ndp);
	ndp_msgrcv_handler_unregister(ndp, &msgrcv_handler_func, msg_type,
				      ifindex, NULL);
	return err;
}

static int get_msg_type(enum ndp_msg_type *p_msg_type, char *msgtypestr)
{
	if (!msgtypestr)
		*p_msg_type = NDP_MSG_ALL;
	else if (!strcmp(msgtypestr, "rs"))
		*p_msg_type = NDP_MSG_RS;
	else if (!strcmp(msgtypestr, "ra"))
		*p_msg_type = NDP_MSG_RA;
	else if (!strcmp(msgtypestr, "ns"))
		*p_msg_type = NDP_MSG_NS;
	else if (!strcmp(msgtypestr, "na"))
		*p_msg_type = NDP_MSG_NA;
	else if (!strcmp(msgtypestr, "r"))
		*p_msg_type = NDP_MSG_R;
	else
		return -EINVAL;
	return 0;
}

int main(int argc, char **argv)
{
	char *argv0 = argv[0];
	static const struct option long_options[] = {
		{ "help",	no_argument,		NULL, 'h' },
		{ "verbose",	no_argument,		NULL, 'v' },
		{ "msg_type",	required_argument,	NULL, 't' },
		{ "ifname",	required_argument,	NULL, 'i' },
		{ NULL, 0, NULL, 0 }
	};
	int opt;
	struct ndp *ndp;
	char *msgtypestr = NULL;
	enum ndp_msg_type msg_type;
	char *ifname = NULL;
	uint32_t ifindex;
	char *cmd_name;
	int err;
	int res = EXIT_FAILURE;

	while ((opt = getopt_long(argc, argv, "hp:i:t:",
				  long_options, NULL)) >= 0) {

		switch(opt) {
		case 'h':
			print_help(argv0);
			return EXIT_SUCCESS;
		case 'v':
			g_verbosity++;
			break;
		case 't':
			free(msgtypestr);
			msgtypestr = strdup(optarg);
			break;
		case 'i':
			free(ifname);
			ifname = strdup(optarg);
			break;
		case '?':
			pr_err("unknown option.\n");
			print_help(argv0);
			return EXIT_FAILURE;
		default:
			pr_err("unknown option \"%c\".\n", opt);
			print_help(argv0);
			return EXIT_FAILURE;
		}
	}

	if (optind >= argc) {
		pr_err("No command specified.\n");
		print_help(argv0);
		goto errout;
	}

	argv += optind;
	cmd_name = *argv++;
	argc -= optind + 1;

	ifindex = 0;
	if (ifname) {
		ifindex = if_nametoindex(ifname);
		if (!ifindex) {
			pr_err("Interface \"%s\" does not exist\n", ifname);
			goto errout;
		}
	}

	err = get_msg_type(&msg_type, msgtypestr);
	if (err) {
		pr_err("Invalid message type \"%s\" selected\n", msgtypestr);
		print_help(argv0);
		goto errout;
	}

	err = ndp_open(&ndp);
	if (err) {
		pr_err("Failed to open ndp: %s\n", strerror(-err));
		goto errout;
	}

	if (!strncmp(cmd_name, "monitor", strlen(cmd_name))) {
		err = run_cmd_monitor(ndp, msg_type, ifindex);
	} else {
		pr_err("Unknown command \"%s\"\n", cmd_name);
		goto ndp_close;
	}

	if (err) {
		pr_err("Command failed \"%s\"\n", strerror(-err));
		goto ndp_close;
	}

	res = EXIT_SUCCESS;

ndp_close:
	ndp_close(ndp);
errout:
	return res;
}