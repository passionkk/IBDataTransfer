/*
 * RDMAParser.h
 *
 *  Created on: 2015-5-14
 *      Author: hek
 */

#ifndef RDMAPARSER_H_
#define RDMAPARSER_H_

#include "rdma_mt.h"

class RDMAParser
{
public:
	RDMAParser();
	virtual ~RDMAParser();

public:
	bool Parse(struct user_param_t *user_param, char *argv[], int argc);


private:
	void initUserParam(struct user_param_t *user_param);
	void usage(const char *argv0);
	void dumpArgs(struct rdma_resource_t* rdma_resource);
};

#endif /* RDMAPARSER_H_ */
