/* momctl */

/* build w/

cc momctl.c -o momctl -L ../lib/Libnet -L ../lib/Libpbs -lnet -lpbs -I ../include
cc momctl.c -g -ggdb	-o momctl -L ../lib/Libnet -L ../lib/Libpbs -lpthread -lnet -lpbs -I ../include -lcrypto
gcc momctl.c -g -ggdb	-o momctl -L ../lib/Libnet -L ../lib/Libpbs -lnet -lpbs -I ../include -lcrypto -L ../lib/Libtpp -ltpp	-L ../lib/Libutil -lutil -L ../lib/Liblog -llog -lpthread
*/




#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

extern char *optarg;

#include "libpbs.h"
#include "tpp.h"
#include "log.h"
#include "pbs_error.h"
#include "pbs_ifl.h"
#include "resmon.h"
#include "rm.h"

#define MAX_QUERY	128
#define SHOW_NONE 0xff
#define MAX_KILLED_LEN 256

int log_mask;

char *LocalHost = "localhost";
char *DiagPtr	= "diag";
char *killmsg_ptr = "killmsg";

char *Query[MAX_QUERY];
int	 QueryI	 = 0;

char *FPtr		 = NULL;
char *JPtr		 = NULL;

char HostFile[256];
char ConfigBuf[65536];

int MOMPort = 0;	/* use default PBS MOM port */

unsigned char IsVerbose = FALSE;

enum MOMCmdEnum {
	momNONE = 0,
	momClear,
	momQuery,
	momReconfig,
	momShutdown
};

enum MOMCmdEnum CmdIndex = momNONE;

/* prototypes */

void MCShowUsage(char *);
int do_mom(char *, int, int);
/* END prototypes */


int
main(int ArgC, char **ArgV)
{
	const char *OptString = "c:Cd:f:h:p:q:r:k:sv";
	char	HostList[65536];
	char *HPtr;
	int c;
	int HostCount;
	int FailCount;

	/* initialize */

	HostList[0] = '\0';
	ConfigBuf[0] = '\0';

	if (getuid() != 0) {
		fprintf(stderr, "ERROR:	must be root to run this command\n");
		exit(EXIT_FAILURE);
	}

	while ((c = getopt(ArgC, ArgV, OptString)) != EOF) {
		switch (c) {
			case 'c':
				/* clear stale job */
				JPtr = optarg;
				CmdIndex = momClear;
				break;

			case 'C':
				/* force cycle */
				CmdIndex = momQuery;
				Query[QueryI] = strdup("cycle");
				QueryI++;
				break;

			case 'd':
				/* diagnose */
				/* FORMAT:	momctl -d<X> */
				CmdIndex = momQuery;

				if ((Query[QueryI] = calloc(strlen(DiagPtr) + 3, sizeof(char))) == NULL) {
					fprintf(stderr,"ERROR: could not calloc %d bytes!\n", (int)strlen(DiagPtr) + 3);
					exit(EXIT_FAILURE);
				}

				if (optarg == NULL) {
					strncpy(Query[QueryI],DiagPtr,strlen(DiagPtr));
				} else {
					snprintf(Query[QueryI],strlen(DiagPtr) + 2,"%s%s", DiagPtr, optarg);
				}

				QueryI++;
				break;

			case 'f':
				{
					int	 rc;
					FILE *fp;
					long size;

					if ((fp = fopen(optarg, "r")) == NULL) {
						fprintf(stderr, "ERROR: cannot open file '%s', errno: %d (%s)\n",
							optarg,
							errno,
							strerror(errno));
						exit(EXIT_FAILURE);
					}

					rc = fread(HostList, sizeof(HostList), 1, fp);

					if ((rc == 0) && (!feof(fp))) {
						fprintf(stderr, "ERROR: cannot read file '%s', errno: %d (%s)\n",
							optarg,
							errno,
							strerror(errno));
						exit(EXIT_FAILURE);
					}
					size = ftell(fp);
					HostList[MIN(size,(long)sizeof(HostList) - 1)] = '\0';
					fclose(fp);
				}	/* END BLOCK */
				break;

			case 'h':
				/* connect to specified host */
				strncpy(HostList,optarg,sizeof(HostList));
				break;

			case 'p':
				/* port */
				if (optarg == NULL)
					MCShowUsage("port not specified");
				MOMPort = (int)strtol(optarg, NULL, 10);
				if (MOMPort == 0)
					MCShowUsage("invalid port specified");
				break;

			case 'q':
				/* query resources */
				if (optarg == NULL) {
					MCShowUsage("query not specified");
					Query[QueryI] = strdup(DiagPtr);
				} else {
					Query[QueryI] = strdup(optarg);
				}
				QueryI++;
				CmdIndex = momQuery;
				break;

			case 'r':
				/* reconfigure */
				{
					CmdIndex = momReconfig;

					/* NOTE:	specify remote file to load -> 'fname' */
					/*				specify local file to stage -> 'LOCAL:fname' */

					if (optarg == NULL)
						MCShowUsage("file not specified");

					if (!strncmp(optarg, "LOCAL:", strlen("LOCAL:"))) {
						FILE *fp;
						int	 size;
						int	 rc;
						char *ptr;
						char *cptr;

						strcpy(ConfigBuf, "CONFIG:");
						cptr = ConfigBuf + strlen(ConfigBuf);
						ptr = optarg + strlen("LOCAL:");

						if ((fp = fopen(ptr, "r")) == NULL) {
							fprintf(stderr, "ERROR: cannot open file '%s', errno: %d (%s)\n", optarg, errno, strerror(errno));
							exit(EXIT_FAILURE);
						}

						rc = fread(cptr, sizeof(ConfigBuf) - strlen(ConfigBuf), 1, fp);

						if ((rc == 0) && (!feof(fp))) {
							fprintf(stderr, "ERROR: cannot read file '%s', errno: %d (%s)\n", optarg, errno, strerror(errno));
							exit(EXIT_FAILURE);
						}

						size = ftell(fp);
						ConfigBuf[MIN(size + strlen("CONFIG:"),sizeof(ConfigBuf) - 1)] = '\0';
						fclose(fp);
					} else {
						strncpy(ConfigBuf, optarg, sizeof(ConfigBuf));
					}
				}	/* END (case 'r') */
				break;

			case 's':
				/* shutdown */
				CmdIndex = momShutdown;
				break;

			case 'k':
				/* kill msg */
				/* FORMAT:	momctl -k <PID|JOBID>:<msg> */
				CmdIndex = momQuery;

				if ((Query[QueryI] = calloc(MAX_KILLED_LEN, sizeof(char))) == NULL) {
					fprintf(stderr,"ERROR: could not calloc %d bytes!\n", (int)strlen(DiagPtr) + 3);
					exit(EXIT_FAILURE);
				}

				if (optarg == NULL)
					exit(EXIT_FAILURE);

				snprintf(Query[QueryI], MAX_KILLED_LEN, "%s=%s", killmsg_ptr, optarg);

				char *ptr = strchr(Query[QueryI], '=') + 1;
				if (*ptr == '\0') {
					fprintf(stderr, "ERROR: invalid format: '%s', requested format: '<PID|JOBID>:<msg>'\n", optarg);
					exit(EXIT_FAILURE);
				}

				if (!strchr(ptr, ':')) {
					fprintf(stderr, "ERROR: invalid format: '%s', requested format: '<PID|JOBID>:<msg>'\n", optarg);
					exit(EXIT_FAILURE);
				}

				ptr = strchr(ptr, ':') + 1;
				if (*ptr == '\0') {
					fprintf(stderr, "ERROR: invalid format: '%s', requested format: '<PID|JOBID>:<msg>'\n", optarg);
					exit(EXIT_FAILURE);
				}

				QueryI++;
				break;

			case 'v':

				/* report verbose logging */
				IsVerbose = TRUE;
				break;
		}	/* END switch (c) */
	}		/* END while (c = getopt()) */

	if (CmdIndex == momNONE) {
		MCShowUsage("no command specified");
	}

	if (HostList[0] == '\0')
		strcpy(HostList, LocalHost);

	HPtr = strtok(HostList, ", \t\n");
	HostCount = 0;
	FailCount = 0;

	/* at this point, all args processing and setup is completed ...
	 * ... now we run through each comma-delimited word in HPtr */

	while (HPtr != NULL) {
		if ((*HPtr == ':') && (*(HPtr + 1) != '\0')) {
			/* finds nodes with this property */

			int con;
			char *def_server, *pserver, *servername;
			struct batch_status *bstatus, *pbstat;
			struct attrl *nodeattrs;

			def_server = pbs_default();

			if ((pserver = strchr(HPtr,'@')) != NULL) {
				*pserver = '\0';
				servername = pserver + 1;
			} else {
				servername = def_server;
			}

			con = pbs_connect(servername);

			if (con < 0) {
				fprintf(stderr,"failed to connect to pbs_server:%s\n", servername);
				exit(EXIT_FAILURE);
			}

			/* get a batch_status entry for each node in ":property" */

			bstatus = pbs_statnode(con,HPtr,NULL,NULL);
			if (bstatus != NULL) {
				for (pbstat = bstatus;pbstat != NULL;pbstat = pbstat->next) {
					/* check state first, only do_mom() if not down */
					for (nodeattrs = pbstat->attribs;nodeattrs != NULL; nodeattrs = nodeattrs->next) {
						if (!strcmp(nodeattrs->name, ATTR_NODE_state)) {
							if (!strstr(nodeattrs->value, ND_down)) {
								do_mom(pbstat->name, MOMPort, CmdIndex) >= 0 ? HostCount++ : FailCount++;
							} else {
								fprintf(stderr,"%12s:	 skipping down node\n", pbstat->name);
							}
							break;
						}	/* END if (attrib name eq state) */
					}		/* END for (nodeattrs) */
				}			/* END for (pbstat) */

				pbs_statfree(bstatus);
			} else {
				fprintf(stderr,"no nodes found in %s on %s\n", HPtr, servername);
			}

			pbs_disconnect(con);

			if (pserver != NULL)
				*pserver = '@';
		} else {
			do_mom(HPtr, MOMPort, CmdIndex) >= 0 ? HostCount++ : FailCount++;
		} /* END if (*HPtr == ':') */

		HPtr = strtok(NULL, ", \t\n");
	}	/* END while (HPtr != NULL) */


	if (IsVerbose == TRUE) {
		fprintf(stdout, "Node Summary:	%d Successful	%d Failed\n", HostCount, FailCount);
	}

	/* SUCCESS */
	exit(EXIT_SUCCESS);
}	/* END main() */





int
do_mom(char *HPtr, int MOMPort, int CmdIndex)
{
	int sd;
	char mom_name[PBS_MAXHOSTNAME+1];
	int mom_port = 0;
	int rc, c;

	if (gethostname(mom_name, (sizeof(mom_name) - 1)) < 0)
		mom_name[0] = '\0';

	/* load the pbs conf file */
	if (pbs_loadconf(0) == 0) {
		fprintf(stderr, "Configuration error\n");
		return (1);
	}
	
	char 			*nodename;
	struct			tpp_config tpp_conf;
	char			my_hostname[PBS_MAXHOSTNAME+1];
	fd_set 			selset;
	struct 			timeval tv;

	if (pbs_conf.pbs_leaf_name) {
		nodename = pbs_conf.pbs_leaf_name;
	} else {
		if (gethostname(my_hostname, (sizeof(my_hostname) - 1)) < 0) {
			fprintf(stderr, "Failed to get hostname\n");
			return -1;
		}
		nodename = my_hostname;
	}

	/* We don't want to show logs related to connecting pbs_comm on console
		* this set this flag to ignore it
		*/
	log_mask = SHOW_NONE;

	/* call tpp_init */
	rc = 0;
	rc = set_tpp_config(&pbs_conf, &tpp_conf, nodename, -1, pbs_conf.pbs_leaf_routers);
	if (rc == -1) {
		fprintf(stderr, "Error setting TPP config\n");
		return -1;
	}

	if ((tpp_fd = tpp_init(&tpp_conf)) == -1) {
		fprintf(stderr, "rpp_init failed\n");
		return -1;
	}

	/*
		* Wait for net to get restored, ie, app to connect to routers
		*/
	FD_ZERO(&selset);
	FD_SET(tpp_fd, &selset);
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	select(FD_SETSIZE, &selset, (fd_set *) 0, (fd_set *) 0, &tv);

	tpp_poll(); /* to clear off the read notification */

	/* Once the connection is established we can unset log_mask */
	log_mask &= ~SHOW_NONE;

	/* get the FQDN of the mom */
	c = get_fullhostname(mom_name, mom_name, (sizeof(mom_name) - 1));
	if (c == -1) {
		fprintf(stderr, "Unable to get full hostname for mom %s\n", mom_name);
		return -1;
	}

	HPtr = strdup(mom_name);
	MOMPort = mom_port;

	if ((sd = openrm(HPtr, MOMPort)) < 0) {
		/* FAILURE */

		//extern char TRMEMsg[];
		char TRMEMsg[1024];	/* rm error message */

		fprintf(stderr, "cannot connect to MOM on node '%s', errno=%d (%s)\n", HPtr, pbs_errno, strerror(pbs_errno));

		if (TRMEMsg[0] != '\0') {
			fprintf(stderr, " %s\n", TRMEMsg);
		}

		return(sd);
	}

	if (IsVerbose == TRUE) {
		fprintf(stderr, "INFO: successfully connected to %s\n", HPtr);
	}

	switch (CmdIndex) {
		case momClear:
			{
				char tmpLine[1024];
				char *Value;
				snprintf(tmpLine, 1024, "clearjob=%s", (JPtr != NULL) ? JPtr : "all");

				if (addreq(sd, tmpLine) != 0) {
					/* FAILURE */
					fprintf(stderr,"ERROR: cannot request job clear on %s (errno=%d: %d)\n", HPtr, errno, pbs_errno);
					closerm(sd);
					return(EXIT_FAILURE);
				}

				if ((Value = (char *)getreq(sd)) == NULL) {
					/* FAILURE */
					fprintf(stderr,"ERROR: job clear failed on %s (errno=%d: %d)\n", HPtr, errno, pbs_errno);
					closerm(sd);
					return(EXIT_FAILURE);
				}

				if (strlen(Value) == 0) {
					/* FAILURE */
					fprintf(stderr,"ERROR: job clear failed on %s\n", HPtr);
					closerm(sd);
					return(EXIT_FAILURE);
				}

				/* job cleared */

				fprintf(stdout,"job clear request successful on %s\n", HPtr);
			}	/* END BLOCK (case momClear) */
			break;

		case momShutdown:
			{
				int rc;
				rc = downrm(sd);
				if (rc != 0) {
					/* FAILURE */
					fprintf(stderr,"ERROR: cannot shutdown mom daemon on %s (errno=%d: %d)\n", HPtr, errno, pbs_errno);
					closerm(sd);
					exit(EXIT_FAILURE);
				}

				fprintf(stdout, "shutdown request successful on %s\n", HPtr);
			}	/* END BLOCK */
			break;

		case momReconfig:
			{
				int rc;
				rc = configrm(sd, ConfigBuf);
				if (rc != 0) {
					/* FAILURE */
					fprintf(stderr,"ERROR: cannot reconfigure mom on %s (errno=%d: %d)\n", HPtr, errno, pbs_errno);
					closerm(sd);
					return(EXIT_FAILURE);
				}

				fprintf(stdout, "reconfig successful on %s\n", HPtr);
			}	/* END BLOCK (case momReconfig) */
			break;

		case momQuery:

		default:
			{
				char *ptr;
				int	rindex;
				char *Value;
				int was_error = 0;
				for (rindex = 0;rindex < QueryI;rindex++) {
					if (addreq(sd, Query[rindex]) != 0) {
						fprintf(stderr,"ERROR: cannot add query for '%s' on %s (errno=%d: %d)\n", Query[rindex], HPtr, errno, pbs_errno);
						was_error = 1;
					}
				}

				for (rindex = 0;rindex < QueryI;rindex++) {
					if ((ptr = strchr(Query[rindex],'=')) != NULL) {
						*ptr = '\0';
					}

					if ((Value = (char *)getreq(sd)) == NULL) {
						fprintf(stderr, "ERROR:	query[%d] '%s' failed on %s (errno=%d: %d)\n", rindex, Query[rindex], HPtr, errno, pbs_errno);
						was_error = 1;
					} else {
						if (!strncmp(Query[rindex], "diag", strlen("diag"))) {
							fprintf(stdout, "%s\n", Value);
						} else if (!strncmp(Query[rindex], "cycle", strlen("cycle"))) {
							fprintf(stdout, "mom %s successfully cycled %s\n", HPtr, Value);
						} else {
							fprintf(stdout, "%12s: %12s = '%s'\n", HPtr, Query[rindex], Value);
						}
					}

					if (ptr != NULL) {
						*ptr = '=';
					}
				}	/* END for (rindex) */
				closerm(sd);
				if (was_error)
					return (was_error);
			}	/* END BLOCK (case momQuery) */
			break;
		}	/* END switch(CmdIndex) */

	closerm(sd);
	return(0);
	} /* END do_mom() */

void
MCShowUsage(char *Msg)	/* I (optional) */
{
	if (Msg != NULL)
		fprintf(stderr, "	%s\n", Msg);

	fprintf(stderr, "USAGE:	momctl <ARGS>\n");
	fprintf(stderr, "		[ -c {JOB|'all'} ] // CLEAR STALE JOB\n");
	fprintf(stderr, "		[ -C ]             // CYCLE\n");
	fprintf(stderr, "		[ -d DIAGLEVEL ]   // DIAGNOSE (0 - 3)\n");
	fprintf(stderr, "		[ -f HOSTFILE ]    // FILE CONTAINING HOSTLIST\n");
	fprintf(stderr, "		[ -h HOST[,HOST]... ] // HOSTLIST\n");
	fprintf(stderr, "		[ -p PORT ]        // PORT\n");
	fprintf(stderr, "		[ -q ATTR ]        // QUERY ATTRIBUTE\n");
	fprintf(stderr, "		[ -r FILE ]        // RECONFIG\n");
	fprintf(stderr, "		[ -s ]             // SHUTDOWN\n");
	fprintf(stderr, "\n");
	fprintf(stderr, " Only one of c, C, d, q, r, or s must be specified, but -q may\n");
	fprintf(stderr, " be used multiple times. HOST may be a hostname or \":property\".\n");
	exit(EXIT_FAILURE);
}	/* END MCShowUsage() */

/* END momctl.c */
