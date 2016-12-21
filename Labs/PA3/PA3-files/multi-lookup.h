

#ifndef MULTI_LOOKUP_H
#define MULTI_LOOKUP_H


#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "util.h"
#include "queue.h"

#define MINARGS 3
#define USAGE "<inputFilePath> <outputFilePath>"
#define SBUFSIZE 1025
#define INPUTFS "%1024s"

// Producer hostname push
void* read_file(char* filename);

// Pool for producers, thread creation
void* producer_pool(char* input_files);

// resolve dns
void* resolve_dns();

// Pool for consumers, thread creation
void* consumer_pool();

// main
int main(int argc, char* argv[]);

#endif