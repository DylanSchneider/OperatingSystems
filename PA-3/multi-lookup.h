//
//  multi-lookup.h
//  multi-lookup
//
//  Created by Dylan Schneider on 3/12/17.
//  Copyright © 2017 Dylan Schneider. All rights reserved.
//

#ifndef multi_lookup_h
#define multi_lookup_h

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "queue.h"
#include "util.h"

#define MINARGS 3
#define DOMAIN_SIZE 1024
#define MAX_NAME_LIMIT 225
#define INPUTFS "%1024s"
#define QUEUE_SIZE 50


void* readerPool(char** inFiles);
void* Read(char* fileName);

void* resolverPool();
void* Resolve();

#endif /* multi_lookup_h */
