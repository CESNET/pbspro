/*
 * Copyright (C) 1994-2016 Altair Engineering, Inc.
 * For more information, contact Altair at www.altair.com.
 *  
 * This file is part of the PBS Professional ("PBS Pro") software.
 * 
 * Open Source License Information:
 *  
 * PBS Pro is free software. You can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free 
 * Software Foundation, either version 3 of the License, or (at your option) any 
 * later version.
 *  
 * PBS Pro is distributed in the hope that it will be useful, but WITHOUT ANY 
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Affero General Public License for more details.
 *  
 * You should have received a copy of the GNU Affero General Public License along 
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 * Commercial License Information: 
 * 
 * The PBS Pro software is licensed under the terms of the GNU Affero General 
 * Public License agreement ("AGPL"), except where a separate commercial license 
 * agreement for PBS Pro version 14 or later has been executed in writing with Altair.
 *  
 * Altair’s dual-license business model allows companies, individuals, and 
 * organizations to create proprietary derivative works of PBS Pro and distribute 
 * them - whether embedded or bundled with other software - under a commercial 
 * license agreement.
 * 
 * Use of Altair’s trademarks, including but not limited to "PBS™", 
 * "PBS Professional®", and "PBS Pro™" and Altair’s logos is subject to Altair's 
 * trademark licensing policies.
 *
 */

/**
 * @file	ecl_verify_values.c
 *
 * @brief	The attribute value verification functions
 *
 * @par Functionality:
 *	This module contains the attribute value verification functions.\n
 *	Each function in this module takes a common format as follows:\n
 *
 * @par Signature:
 *	int verify_value_xxxx(int batch_request, int parent_object,
 *                      struct attropl * pattr, char **err_msg)\n
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 - Attribute passed verification
 * @retval	>0 - Failed verification - pbs errcode is returned
 *
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <stdlib.h>
#include <string.h>

#include "pbs_ifl.h"
#include "pbs_ecl.h"
#include "pbs_error.h"
#include "libpbs.h"
#include "cmds.h"
#include "ticket.h"
#include "pbs_license.h"

#include "job.h"
#include "reservation.h"
#include "queue.h"
#include "pbs_nodes.h"
#include "server.h"
#include "batch_request.h"
#include "pbs_share.h"

static long ecl_pbs_max_licenses = PBS_MAX_LICENSING_LICENSES;

/**
 * @brief
 *	verify the datatype and value of a resource
 *
 * @par Functionality:
 *	1. Call ecl_find_resc_def to find the resource defn\n
 *	2. Call at_verify_datatype to verify the datatype of the resource\n
 *	3. Call at_verify_value to verify the value of a resource
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 - Attribute passed verification
 * @retval	>0 - Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	Some functions reset the value pointer to a new value. It thus
 *	frees the old value pointer.
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_resc(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	ecl_attribute_def *prdef;
	int err_code = PBSE_NONE;
	char *p;

	struct attropl resc_attr;

	if (pattr == NULL)
		return (PBSE_INTERNAL);

	if (pattr->resource == (char *) 0)
		return (PBSE_NONE);

	if ((prdef = ecl_find_resc_def(ecl_svr_resc_def, pattr->resource,
		ecl_svr_resc_size))) {
		/* found the resource, verify type and value of resource */
		resc_attr.name = pattr->resource;
		resc_attr.value = pattr->value;

		if (prdef->at_verify_datatype)
			err_code = prdef->at_verify_datatype(&resc_attr,
				err_msg);

		if ((err_code == 0) && (prdef->at_verify_value)) {
			err_code = prdef->at_verify_value(batch_request,
				parent_object, cmd, &resc_attr, err_msg);
		}
		if ((err_code != 0) && (*err_msg == NULL)) {
			p = pbse_to_txt(err_code);
			if (p) {
				*err_msg = malloc(strlen(p)
					+ strlen(pattr->name)
					+ strlen(pattr->resource)
					+ 3);
				if (*err_msg == NULL) {
					err_code = PBSE_SYSTEM;
					return -1;
				}
				sprintf(*err_msg, "%s %s.%s",
					p, pattr->name, pattr->resource);
			}
		}
	}
	/*
	 * unknown resources are okay at this point of time
	 * we dont return error if resource is not found
	 * since custom resources are known only to server
	 * and verified by server
	 */
	return err_code;
}

/**
 * @brief
 *	Verify function for user_list (ATTR_g)
 *
 * @par Functionality:
 *	verify function for the user/group lists related attributes(ATTR_g)\n
 *	calls parse_at_list
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_user_list(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	if (batch_request == PBS_BATCH_SelectJobs) {
		if (parse_at_list(pattr->value, FALSE, FALSE))
			return PBSE_BADATVAL;
	} else {
		if (parse_at_list(pattr->value, TRUE, FALSE))
			return PBSE_BADATVAL;
	}
	return PBSE_NONE;
}

/**
 * @brief
 *	Verify authorized users (ATTR_auth_u/g)
 *
 * @par Functionality:
 *	verify function for the attributes ATTR_auth_u, ATTR_auth_g\n
 *	calls parse_at_list to parse the list of values
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_authorized_users(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	if (parse_at_list(pattr->value, FALSE, FALSE))
		return PBSE_BADATVAL;

	return PBSE_NONE;
}

/**
 * @brief
 *	verify function for the attributes ATTR_depend
 *
 * @par Functionality:
 *	verify function for the attributes ATTR_depend\n
 *	calls parse_depend_list to parse the list of job dependencies\n
 *	NOTE: This function also resets the value pointer to the new (expanded)
 *	dependancy list. It frees the original value pointer
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_dependlist(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	char *pdepend;

	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	pdepend = malloc(PBS_DEPEND_LEN);
	if (pdepend == NULL)
		return -1;

	if (parse_depend_list(pattr->value, &pdepend, PBS_DEPEND_LEN)) {
		free(pdepend);
		return PBSE_BADATVAL;
	}
	/* replace the value with the expanded value */
	free(pattr->value);
	pattr->value = pdepend;
	return PBSE_NONE;
}

/**
 * @brief
 *	verify function for the attributes ATTR_o, ATTR_e etc
 *
 * @par Functionality
 *	calls prepare_path to parse the path associatedd with ATTR_o, ATTR_e etc
 *	NOTE: This function also resets the value pointer to the new (expanded)
 *	dependancy list. It frees the original value pointer
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_path(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	char *path_out;
	path_out = malloc(MAXPATHLEN + 1);
	if (path_out == NULL)
		return PBSE_SYSTEM;
	else
		memset(path_out, 0, MAXPATHLEN+1);

	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	if (prepare_path(pattr->value, path_out) != 0) {
		free(path_out);
		return (PBSE_BADATVAL);
	}
	/* replace with prepared path */
	free(pattr->value);
	pattr->value = path_out;
	return PBSE_NONE;
}

/**
 * @brief
 *	verify function for the attributes ATTR_J
 *
 * @par Functionality:
 *	verify function for the attributes ATTR_J\n
 *	calls chk_Jrange to verify that the range of the value is proper
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_jrange(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	int ret;

	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	ret = chk_Jrange(pattr->value);
	if (ret == 1)
		return PBSE_BADATVAL;
	else if (ret == 2)
		return PBSE_ATVALERANGE;

	return PBSE_NONE;
}

/**
 * @brief
 * verify function for the attributes ATTR_N (job or resv)
 *
 * @par Functionality:
 *	verify function for the attributes ATTR_N (job or resv)\n
 *	calls check_job_name to verify that the job/resv name is proper
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_jobname(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	int chk_alpha = 1; /* by default disallow numeric first char  */
	int ret;

	if (pattr->value == NULL)
		return PBSE_BADATVAL;

	if (pattr->value[0] == '\0') {
		if ((batch_request == PBS_BATCH_StatusJob) ||
			(batch_request == PBS_BATCH_SelectJobs))
			return PBSE_NONE;
		else
			return PBSE_BADATVAL;
	}

	if (batch_request == PBS_BATCH_QueueJob || 		/* for queuejob allow numeric first char */
		batch_request == PBS_BATCH_ModifyJob || 	/* for alterjob allow numeric first char */
		batch_request == PBS_BATCH_SubmitResv ||	/* for reservation submit allow numeric first char */
		batch_request == PBS_BATCH_SelectJobs)		/* for selectjob allow numeric first char */
		chk_alpha = 0; 

	ret = check_job_name(pattr->value, chk_alpha);
	if (ret == -1)
		return PBSE_BADATVAL;
	else if (ret == -2)
		return PBSE_JOBNBIG;

	return PBSE_NONE;
}

/**
 * @brief
 *	verify function for the attributes ATTR_c (checkpoint)
 *
 * @par Functionality:
 *	verify function for the attributes ATTR_c (checkpoint)\n
 *	Checks that the format of ATTR_c is proper
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_checkpoint(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	char *val = pattr->value;
	char *pc;

	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	pc = val;
	if (strlen(val) == 1) {
		/* include 'u' as a valid one since unset is set as 'u' */
		if (*pc != 'n' && *pc != 's' && *pc != 'c'
			&& *pc != 'w' && *pc != 'u')
			return PBSE_BADATVAL;
	} else {
		if (((*pc != 'c') && (*pc != 'w')) || (*(pc+1) != '='))
			return PBSE_BADATVAL;

		pc += 2;
		if (*pc == '\0')
			return PBSE_BADATVAL;

		while (isdigit(*pc))
			pc++;
		if (*pc != '\0')
			return PBSE_BADATVAL;
	}

	if (batch_request == PBS_BATCH_SelectJobs) {
		if (strcmp(pc, "u") == 0) {
			if ((pattr->op != EQ) && (pattr->op != NE))
				return PBSE_BADATVAL;
		}
	}
	return PBSE_NONE;
}

/**
 * @brief
 *	verify function for the attributes ATTR_h (hold)
 *
 * @par Functionality:
 *	verify function for the attributes ATTR_h (hold)\n
 *	Checks that the format of ATTR_h is proper
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_hold(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	char *val = pattr->value;
	char *pc;
	int u_cnt = 0;
	int o_cnt = 0;
	int s_cnt = 0;
	int p_cnt = 0;
	int n_cnt = 0;

	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	for (pc = val; *pc != '\0'; pc++) {
		if (*pc == 'u')
			u_cnt++;
		else if (*pc == 'o')
			o_cnt++;
		else if (*pc == 's')
			s_cnt++;
		else if (*pc == 'p')
			p_cnt++;
		else if (*pc == 'n')
			n_cnt++;
		else
			return (PBSE_BADATVAL);
	}
	if (n_cnt && (u_cnt + o_cnt + s_cnt + p_cnt))
		return (PBSE_BADATVAL);
	if (p_cnt && (u_cnt + o_cnt + s_cnt + n_cnt))
		return (PBSE_BADATVAL);

	return PBSE_NONE;
}

/**
 * @brief
 *	verify function for the attributes ATTR_j
 *
 * @par Functionality:
 *	verify function for the attributes ATTR_j\n
 *	Checks that the format of ATTR_j is proper
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_joinpath(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	if (strcmp(pattr->value, "oe") != 0 &&
		strcmp(pattr->value, "eo") != 0 &&
		strcmp(pattr->value, "n") != 0) {

		return PBSE_BADATVAL;
	}
	return PBSE_NONE;
}

/**
 * @brief
 *	verify function for the attributes ATTR_k
 *
 * @par Functionality:
 *	verify function for the attributes ATTR_k\n
 *	Checks that the format of ATTR_k is proper
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_keepfiles(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	if ((strcmp(pattr->value, "o") != 0) &&
		(strcmp(pattr->value, "e") != 0) &&
		(strcmp(pattr->value, "oe") != 0) &&
		(strcmp(pattr->value, "eo") != 0) &&
		(strcmp(pattr->value, "n") != 0)) {

		return PBSE_BADATVAL;
	}
	return PBSE_NONE;
}

/**
 * @brief
 *	verify function for the attributes ATTR_m (mailpoints)
 *
 * @par Functionality:
 *	verify function for the attributes ATTR_m (mailpoints)\n
 *	Checks that the format of ATTR_m is proper
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_mailpoints(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	char *pc;

	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	while (isspace((int) * pattr->value)) pattr->value++;
	if (strlen(pattr->value) == 0)
		return PBSE_BADATVAL;

	if (strcmp(pattr->value, "n") != 0) {
		pc = pattr->value;
		while (*pc) {
			if (batch_request == PBS_BATCH_SubmitResv) {
				if (*pc != 'a' && *pc != 'b' && *pc != 'e'
					&& *pc != 'c')
					return PBSE_BADATVAL;
			} else {
				if (*pc != 'a' && *pc != 'b' && *pc != 'e')
					return PBSE_BADATVAL;
			}
			pc++;
		}
	}
	return PBSE_NONE;
}

/**
 * @brief
 *	verify function for the attributes ATTR_M (mailusers)
 *
 * @par Functionality:
 *	verify function for the attributes ATTR_M (mailusers)\n
 *	Checks that the format of ATTR_M is proper by calling parse_at_list
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_mailusers(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	if (parse_at_list(pattr->value, FALSE, FALSE))
		return PBSE_BADATVAL;

	return PBSE_NONE;
}

/**
 * @brief
 *	verify function for the attributes ATTR_S
 *
 * @par Functionality:
 *	verify function for the attributes ATTR_S\n
 *	Checks that the format of ATTR_S is proper by calling parse_at_list
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_shellpathlist(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	if (parse_at_list(pattr->value, TRUE, TRUE))
		return PBSE_BADATVAL;

	return PBSE_NONE;
}

/**
 * @brief
 *	verify function for the attributes ATTR_p
 *
 * @par Functionality:
 *	verify function for the attributes ATTR_p\n
 *	Checks that the format of ATTR_p is proper (between -1024 & +1023)
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_priority(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	int i;

	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	i = atoi(pattr->value);
	if (i < -1024 || i > 1023) {
		if (batch_request == PBS_BATCH_SelectJobs)
			return PBSE_NONE;
		else
			return PBSE_BADATVAL;
	}

	return PBSE_NONE;
}

/**
 * @brief
 *	verify function for the attributes ATTR_sandbox
 *
 * @par Functionality:
 *	verify function for the attributes ATTR_sandbox
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_sandbox(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	if ((strcasecmp(pattr->value, "HOME") != 0) &&
		(strcasecmp(pattr->value, "O_WORKDIR") != 0) &&
		(strcasecmp(pattr->value, "PRIVATE") != 0)) {
		return PBSE_BADATVAL;
	}

	return PBSE_NONE;
}

/**
 * @brief
 *	 verify function for the attributes ATTR_stagein, ATTR_stageout
 *	Checks that the format by calling parse_stage_list
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_stagelist(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	if (parse_stage_list(pattr->value))
		return PBSE_BADATVAL;

	return PBSE_NONE;
}

/**
 * @brief
 *	verify function for the attributes ATTR_ReqCred
 *	Checks that the value is one of the allowed values
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_credname(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	static const char *cred_list[] = {
		PBS_CREDNAME_AES,
		PBS_CREDNAME_DCE_KRB5,
		PBS_CREDNAME_KRB5,
		PBS_CREDNAME_GRIDPROXY,
		NULL /* must be last */
	};

	char *val = pattr->value;
	int i;

	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	for (i = 0; cred_list[i]; i++) {
		if (strcmp(cred_list[i], val) == 0)
			return PBSE_NONE;
	}
	return PBSE_BADATVAL;
}

/**
 * @brief
 *	 for some attributes which can have 0 or +ve values like ATTR_rpp_retry
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_zero_or_positive(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	long l;

	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	l = atol(pattr->value);
	if (l < 0)
		return PBSE_BADATVAL;

	return PBSE_NONE;
}

/**
 * @brief
 *	Function checks the resource "preempt_targets" and verifies its values
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 */
int
verify_value_preempt_targets(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	char * val = NULL;
	char * result = NULL;
	char *temp = NULL;
	char *p = NULL;
	char *q = NULL;
	char ch = 0;
	char ch1 = 0;
	int err_code = PBSE_NONE;
	ecl_attribute_def *prdef = NULL;
	char *value = NULL;
	char *msg = NULL;
	int attrib_found= 0;
	char *lcase_val = NULL;
	char *chk_arr[] = {ATTR_l, ATTR_queue, NULL};
	int i = 0;
	int j = 0;
	int res_len = 0;
	ecl_attribute_def *ecl_def = ecl_svr_resc_def;
	int ecl_def_size = ecl_svr_resc_size;
	struct attropl resc_attr = {0};

	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;
	val = pattr->value;
	while (isspace(*val))
		val++;
	/* Check if preempt_targets is set to "NONE" */
	if (strncasecmp(val, TARGET_NONE, strlen(TARGET_NONE)) == 0) {
		if (strcasecmp(val, TARGET_NONE) != 0)
			err_code = PBSE_BADATVAL;
		return err_code;
	}
	for (i = 0; chk_arr[i] != NULL; i++) {
		if (strcmp(chk_arr[i], ATTR_queue) == 0) {
			ecl_def = ecl_resv_attr_def;
			ecl_def_size = ecl_resv_attr_size;
			/*
			 * Implementation for case insensitive search of string "queue", as many
			 * platforms like AIX, HP-UX and Windows does not have any case
			 * insensitive string search
			 */
			if (lcase_val != NULL) {
				free(lcase_val);
				lcase_val = NULL;
			}
			lcase_val = strdup(val);
			if (lcase_val == NULL)
				return PBSE_SYSTEM;
			for (j = 0; lcase_val[j] != '\0'; j++) {
				lcase_val[j] = tolower(lcase_val[j]);
			}
			val = lcase_val;
		}
		else
			val = pattr->value;
		/* Check preempt_targets for one of the attrib names in its values */
		result = strstr(val, chk_arr[i]);
		res_len = strlen(chk_arr[i]);
		/* Traverse through the values, it may have multiple comma seperated values */
		while (result != NULL) {
			/*At least one of the recognized attributes was found */
			attrib_found = 1;
			if (strcmp(chk_arr[i], ATTR_l) == 0) {
				/* We need to skip "Resource_List" */
				temp = result + res_len;
				if (*temp != '.') {
					free(lcase_val);
					return PBSE_BADATVAL;
				}
				/* Ignoring '.' character */
				temp = temp + 1;
			}
			else
				temp = result;
			p = strpbrk(temp, "=");
			if (p == NULL) {
				free(lcase_val);
				return PBSE_BADATVAL;
			}
			ch = *p;
			value = p+1;
			*p = '\0';
			/* find the resource definition */
			prdef = ecl_find_resc_def(ecl_def, temp, ecl_def_size);
			if (prdef == NULL) {
				*p = ch;
				/* Assuming custom resource, don't know datatype to verify */
				result = strstr(temp, chk_arr[i]);
				continue;
			}
			q = strchr(value,',');
			if (q != NULL) {
				ch1 = *q;
				*q = '\0';
			}
			resc_attr.name = strdup(temp);
			if (resc_attr.name == NULL) {
				free(lcase_val);
				return PBSE_SYSTEM;
			}
			resc_attr.value = strdup(value);
			if (resc_attr.value == NULL) {
				free(lcase_val);
				free(resc_attr.name);
				return PBSE_SYSTEM;
			}
			if (q != NULL)
				*q = ch1;
			*p = ch;
			if (prdef->at_verify_datatype)
				err_code = prdef->at_verify_datatype(&resc_attr,
					err_msg);

			if ((err_code == 0) && (prdef->at_verify_value)) {
				err_code = prdef->at_verify_value(batch_request,
					parent_object, cmd, &resc_attr, err_msg);
			}
			if ((err_code != 0) && (*err_msg == NULL)) {
				msg = pbse_to_txt(err_code);
				if (msg) {
					*err_msg = malloc(strlen(msg) + 1);
					if (*err_msg == NULL) {
						free(lcase_val);
						return PBSE_SYSTEM;
					}
					sprintf(*err_msg, "%s",
						msg);
				}
				return err_code;
			}
			val = p;
			free(resc_attr.name);
			free(resc_attr.value);
			resc_attr.name = resc_attr.value = NULL;
			result = strstr(val, chk_arr[i]);
		}
	}
	free(lcase_val);
	if (attrib_found == 0)
		err_code = PBSE_BADATVAL;
	return err_code;
}

/**
 * @brief
 *	for some attributes which can have only +ve values, eg, ATTR_rpp_highwater
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_non_zero_positive(int batch_request, int parent_object,
	int cmd, struct attropl *pattr, char **err_msg)
{
	long l;

	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	l = atol(pattr->value);
	if (l <= 0)
		return PBSE_BADATVAL;

	return PBSE_NONE;
}

/**
 * @brief
 *	verifies attribute ATTR_license_min
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_minlicenses(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	long l;

	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	l = atol(pattr->value);
	if ((l < 0) || (l > ecl_pbs_max_licenses))
		return (PBSE_LICENSE_MIN_BADVAL);

	return PBSE_NONE;
}

/**
 * @brief
 *	verifies attribute ATTR_license_max
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_maxlicenses(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	long l;

	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	l = atol(pattr->value);

	if ((l < 0) || (l > ecl_pbs_max_licenses))
		return (PBSE_LICENSE_MAX_BADVAL);

	return PBSE_NONE;
}

/**
 * @brief
 *	verifies attribute ATTR_license_linger
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_licenselinger(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	long l;

	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	l = atol(pattr->value);
	if (l <= 0)
		return (PBSE_LICENSE_LINGER_BADVAL);

	return PBSE_NONE;
}

/**
 * @brief
 *	verifies attributes like ATTR_managers, ATTR_operators etc
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_mgr_opr_acl_check(int batch_request, int parent_object,
	int cmd, struct attropl *pattr, char **err_msg)
{
// with kerberos, we cannot really check validity
#if defined(PBS_SECURITY) && (PBS_SECURITY == KRB5)
	return PBSE_NONE;
#endif
	
	char *dup_val;
	char *token;
	char *entry;
	char *p;
	char *comma;
	int err = PBSE_NONE;
	char hostname[PBS_MAXHOSTNAME + 1];

	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	dup_val = strdup(pattr->value);
	if (!dup_val)
		return -1;

	token = dup_val;
	comma = strchr(token, ',');
	while (token) {
		/* eliminate trailing spaces in token */
		if (comma)
			p = comma;
		else
			p = token + strlen(token);
		while (*--p == ' ' && p != token);
		*(p + 1) = 0;

		/* eliminate spaces in the front */
		while (token && *token == ' ') token++;

		entry = strchr(token, (int) '@');
		if (entry == (char *) 0) {
			err = PBSE_BADHOST;
			break;
		}
		entry++; /* point after the '@' */
		if (*entry != '*') { /* if == * cannot check it any more */

			/* if not wild card, must be fully qualified host */
			if (get_fullhostname(entry, hostname, (sizeof(hostname) - 1))
				|| strncasecmp(entry, hostname, (sizeof(hostname) - 1))) {
					err = PBSE_BADHOST;
					break;
			}
		}

		token = NULL;
		if (comma) {
			token = comma + 1;
			comma = strchr(token, ',');
		}
	}

	free(dup_val);
	return err;
}

/**
 * @brief
 *	verifies the queue type specified by attribute ATTR_q
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_queue_type(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	int i;
	char *name[2] = {"Execution", "Route"};

	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	/* does the requested value match a legal value? */
	for (i = 0; i<2; i++) {
		if (strncasecmp(name[i], pattr->value,
			strlen(pattr->value)) == 0)
			return PBSE_NONE;
	}
	return (PBSE_BADATVAL);
}

/**
 * @brief
 *	verifies job state specified by attribute ATTR_state
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 	- 	Attribute passed verification
 * @retval	>0 	- 	Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	None
 *
 * @par Reentrancy
 *	MT-safe
 */
int
verify_value_state(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	char *pc = pattr->value;

	if (pattr->value == NULL)
		return PBSE_BADATVAL;

	if (pattr->value[0] == '\0') {
		if (batch_request != PBS_BATCH_StatusJob)
			return PBSE_BADATVAL;
	}
	while (*pc) {
		if (*pc != 'E' && *pc != 'H' && *pc != 'Q' &&
			*pc != 'R' && *pc != 'T' && *pc != 'W' &&
			*pc != 'S' && *pc != 'U' && *pc != 'B' &&
			*pc != 'X' && *pc != 'F' && *pc != 'M')
			return PBSE_BADATVAL;
		pc++;
	}
	return PBSE_NONE;
}
/**
 * @brief
 *	Parses select specification and verifies the datatype and value of each resource
 *
 * @par Functionality:
 *	1. Parses select specification by calling parse_chunk function.
 *	2. Decodes each chunk 
 *	3. Calls verify_value_resc for each resource in a chunk.
 *
 * @see
 *
 * @param[in]	batch_request	-	Batch Request Type
 * @param[in]	parent_object	-	Parent Object Type
 * @param[in]	cmd		-	Command Type
 * @param[in]	pattr		-	address of attribute to verify
 * @param[out]	err_msg		-	error message list
 *
 * @return	int
 * @retval	0 - Attribute passed verification
 * @retval	>0 - Failed verification - pbs errcode is returned
 *
 * @par	Side effects:
 * 	Some functions reset the value pointer to a new value. It thus
 *	frees the old value pointer.
 *
 */
int
verify_value_select(int batch_request, int parent_object, int cmd,
	struct attropl *pattr, char **err_msg)
{
	char        *chunk;
	int          nchk;
	int          nelem;
	struct       key_value_pair *pkvp;
	int          rc = 0;
	int          j;
	struct attropl resc_attr;
	if ((pattr->value == NULL) || (pattr->value[0] == '\0'))
		return PBSE_BADATVAL;

	chunk = parse_plus_spec(pattr->value, &rc); /* break '+' seperated substrings */
	if (rc != 0)
		return (rc);

	while (chunk) {
#ifdef NAS
		if (parse_chunk(chunk, 0, &nchk, &nelem, &pkvp, NULL) == 0)
#else
		if (parse_chunk(chunk, &nchk, &nelem, &pkvp, NULL) == 0)
#endif
		{
			for (j = 0; j < nelem; ++j) {
				resc_attr.name = pattr->name;
				resc_attr.resource = pkvp[j].kv_keyw;
				resc_attr.value = pkvp[j].kv_val;
				rc = verify_value_resc(batch_request, parent_object, cmd, &resc_attr, err_msg);
				if (rc > 0)
					return rc;
			}
		} else {
			return PBSE_BADATVAL;
		}
		chunk = parse_plus_spec(NULL, &rc);
		if (rc != 0)
			return (rc);
	} /* while */
	return PBSE_NONE;
}
