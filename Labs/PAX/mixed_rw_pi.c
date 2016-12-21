/*
 * File: rw.c
 * Author: Andy Sayler
 * Project: CSCI 3753 Programming Assignment 3
 * Create Date: 2012/03/19
 * Modify Date: 2012/03/20
 * Description: A small i/o bound program to copy N bytes from an input
 *              file to an output file. May read the input file multiple
 *              times if N is larger than the size of the input file.
 */

/* Include Flags */
#define _GNU_SOURCE

/* System Includes */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sched.h>
#include <sys/wait.h>
#include <math.h>

/* Local Defines */
#define MAXFILENAMELENGTH 80
#define DEFAULT_OUTPUTFILENAMEBASE "rwoutput"
#define DEFAULT_BLOCKSIZE 1024
#define DEFAULT_TRANSFERSIZE 1024*100
#define DEFAULT_ITERATIONS 1000000
#define RADIUS (RAND_MAX / 2)

int policy;
long iterations;

inline double dist(double x0, double y0, double x1, double y1){
    return sqrt(pow((x1-x0),2) + pow((y1-y0),2));
}

inline double zeroDist(double x, double y){
    return dist(0, 0, x, y);
}

int piFunction(){

    long i;
    double x, y;
    double inCircle = 0.0;
    double inSquare = 0.0;
    double pCircle = 0.0;
    double piCalc = 0.0;
    //fprintf(stdout, "New Scheduling Policy: %d\n", sched_getscheduler(0));

    /* Calculate pi using statistical methode across all iterations*/
    for(i=0; i<iterations; i++){
    x = (random() % (RADIUS * 2)) - RADIUS;
    y = (random() % (RADIUS * 2)) - RADIUS;
    if(zeroDist(x,y) < RADIUS){
        inCircle++;
    }
    inSquare++;
    }

    /* Finish calculation */
    pCircle = inCircle/inSquare;
    piCalc = pCircle * 4.0;

    /* Print result */
    fprintf(stdout, "pi = %f\n", piCalc);

    return 0;
}

int rwFunction(char inFN[MAXFILENAMELENGTH], char outFN[MAXFILENAMELENGTH]){

    int rv;
    int inputFD;
    int outputFD;
    char inputFilename[MAXFILENAMELENGTH];
    char outputFilename[MAXFILENAMELENGTH];
    char outputFilenameBase[MAXFILENAMELENGTH];

    ssize_t transfersize = 1024*100;
    ssize_t blocksize = 0;
    char* transferBuffer = NULL;
    ssize_t buffersize;

    ssize_t bytesRead = 0;
    ssize_t totalBytesRead = 0;
    int totalReads = 0;
    ssize_t bytesWritten = 0;
    ssize_t totalBytesWritten = 0;
    int totalWrites = 0;
    int inputFileResets = 0;

    /* Process program arguments to select run-time parameters */
    /* Set supplied transfer size or default if not supplied */
    transfersize = DEFAULT_TRANSFERSIZE;
    /* Set supplied block size or default if not supplied */
    blocksize = DEFAULT_BLOCKSIZE;

    /* Set supplied input filename or default if not supplied */

    strncpy(inputFilename, inFN, MAXFILENAMELENGTH);
    /* Set supplied output filename base or default if not supplied */

    strncpy(outputFilenameBase, outFN, MAXFILENAMELENGTH);

    /* Confirm blocksize is multiple of and less than transfersize*/
    if(blocksize > transfersize){
        fprintf(stderr, "blocksize can not exceed transfersize\n");
        exit(EXIT_FAILURE);
    }
    if(transfersize % blocksize){
        fprintf(stderr, "blocksize must be multiple of transfersize\n");
        exit(EXIT_FAILURE);
    }

    /* Allocate buffer space */
    buffersize = blocksize;
    if(!(transferBuffer = malloc(buffersize*sizeof(*transferBuffer)))){
        perror("Failed to allocate transfer buffer");
        exit(EXIT_FAILURE);
    }

    /* Open Input File Descriptor in Read Only mode */
    if((inputFD = open(inputFilename, O_RDONLY | O_SYNC)) < 0){
        perror("Failed to open input file");
        exit(EXIT_FAILURE);
    }

    /* Open Output File Descriptor in Write Only mode with standard permissions*/
    rv = snprintf(outputFilename, MAXFILENAMELENGTH, "%s",
          outputFilenameBase);
    if(rv > MAXFILENAMELENGTH){
        fprintf(stderr, "Output filenmae length exceeds limit of %d characters.\n",
            MAXFILENAMELENGTH);
        exit(EXIT_FAILURE);
    }
    else if(rv < 0){
    perror("Failed to generate output filename");
        exit(EXIT_FAILURE);
    }
    if((outputFD =
        open(outputFilename,
             O_WRONLY | O_CREAT | O_TRUNC | O_SYNC,
             S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) < 0){
        perror("Failed to open output file");
        exit(EXIT_FAILURE);
    }

    /* Print Status */
    fprintf(stdout, "Reading from %s and writing to %s\n",
        inputFilename, outputFilename);

    /* Read from input file and write to output file*/
    do{
    /* Read transfersize bytes from input file*/
    bytesRead = read(inputFD, transferBuffer, buffersize);
    if(bytesRead < 0){
        perror("Error reading input file");
        exit(EXIT_FAILURE);
    }
    else{
        totalBytesRead += bytesRead;
        totalReads++;
    }

    /* If all bytes were read, write to output file*/
    if(bytesRead == blocksize){
        bytesWritten = write(outputFD, transferBuffer, bytesRead);
        if(bytesWritten < 0){
        perror("Error writing output file");
        exit(EXIT_FAILURE);
        }
        else{
        totalBytesWritten += bytesWritten;
        totalWrites++;
        }
    }
    /* Otherwise assume we have reached the end of the input file and reset */
    else{
        if(lseek(inputFD, 0, SEEK_SET)){
        perror("Error resetting to beginning of file");
        exit(EXIT_FAILURE);
        }
        inputFileResets++;
    }

    }while(totalBytesWritten < transfersize);

    /* Output some possibly helpfull info to make it seem like we were doing stuff */
    fprintf(stdout, "Read:    %zd bytes in %d reads\n",
        totalBytesRead, totalReads);
    fprintf(stdout, "Written: %zd bytes in %d writes\n",
        totalBytesWritten, totalWrites);
    fprintf(stdout, "Read input file in %d pass%s\n",
        (inputFileResets + 1), (inputFileResets ? "es" : ""));
    fprintf(stdout, "Processed %zd bytes in blocks of %zd bytes\n",
        transfersize, blocksize);

    /* Free Buffer */
    free(transferBuffer);

    /* Close Output File Descriptor */
    if(close(outputFD)){
        perror("Failed to close output file");
        exit(EXIT_FAILURE);
    }

    /* Close Input File Descriptor */
    if(close(inputFD)){
        perror("Failed to close input file");
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]){
    if(argc != 4){
        fprintf(stderr, "Incorrect input arguments");
    }

    int policy;
    struct sched_param param;
    long processes;

    /* Set policy if supplied */
    if(!strcmp(argv[1], "SCHED_OTHER")){
        policy = SCHED_OTHER;
    }
    else if(!strcmp(argv[1], "SCHED_FIFO")){
        policy = SCHED_FIFO;
    }
    else if(!strcmp(argv[1], "SCHED_RR")){
        policy = SCHED_RR;
    }
    else{
        fprintf(stderr, "Unhandeled scheduling policy\n");
        exit(EXIT_FAILURE);
    }

    processes = atol(argv[2]);


    /* Set process to max prioty for given scheduler */
    param.sched_priority = sched_get_priority_max(policy);

    /* Set new scheduler policy */
    //fprintf(stdout, "Current Scheduling Policy: %d\n", sched_getscheduler(0));
    //fprintf(stdout, "Setting Scheduling Policy to: %d\n", policy);
    if(sched_setscheduler(0, policy, &param)){
        perror("Error setting scheduler policy");
        exit(EXIT_FAILURE);
    }

    iterations = atol(argv[3]);

    /* Run with different files */
    char outputFNArray[processes][MAXFILENAMELENGTH];
    char inputFNArray[processes][MAXFILENAMELENGTH];


    int i, status;
    pid_t pid, w_pid;
    for (i = 0 ; i < processes ; i++){
        sprintf(inputFNArray[i],"input/InputFile%d.txt",i);
    }

    for (i = 0 ; i < processes ; i++){
        sprintf(outputFNArray[i],"output/OutputFile%d.txt",i);
    }
    /* Run processes */
    for (i = 0 ; i < processes ; i++){
        // Fork it
        if ((pid = fork()) == 0){
            piFunction();
            rwFunction(inputFNArray[i], outputFNArray[i]);
            exit(0);
        }
        while((w_pid = waitpid(-1, &status, 0)) > 0){
            if(!WIFEXITED(status)){
                printf("child failed to terminate");
            }
        }
    }
    return 0;
}