/*
 * File: pager-lru.c
 * Author:       Andy Sayler
 *               http://www.andysayler.com
 * Adopted From: Dr. Alva Couch
 *               http://www.cs.tufts.edu/~couch/
 *
 * Project: CSCI 3753 Programming Assignment 4
 * Create Date: Unknown
 * Modify Date: 2012/04/03
 * Description:
 * 	This file contains an lru pageit
 *      implmentation.
 */

#include <stdio.h> 
#include <stdlib.h>
#include <limits.h>

#include "simulator.h"

void pageit(Pentry q[MAXPROCESSES]) { 
    
    /* This file contains the stub for an LRU pager */
    /* You may need to add/remove/modify any part of this file */

    /* Static vars */
    static int initialized = 0;
    static int tick = 1; // artificial time
    static int timestamps[MAXPROCESSES][MAXPROCPAGES];

    /* Local vars */
    int proctmp;
    int pagetmp;

    int process_counter;
    int page_counter;

    int process_page;
    int max = INT_MAX;

    int l;
    int l_i;


    /* initialize static vars on first run */
    if(!initialized){
    for(proctmp=0; proctmp < MAXPROCESSES; proctmp++){
        for(pagetmp=0; pagetmp < MAXPROCPAGES; pagetmp++){
        timestamps[proctmp][pagetmp] = 0; 
        }
    }
    initialized = 1;
    }
    
    /* TODO: Implement LRU Paging */

    //For all available processes
    for (process_counter = 0; process_counter < MAXPROCESSES; process_counter++)
    {
        //If process is active:
        if (q[process_counter].active)
        {
            //Find the page of that active process
            process_page = q[process_counter].pc / PAGESIZE;

            //If page HAS not been swapped out:
            if (!q[process_counter].pages[process_page])
            {
                //If page cannot be swapped in (must evict page)
                if (!pagein(process_counter, process_page))
                {
                    l = max;
                    l_i = 0;

                    //Find a page to evict (LRU)
                    for (page_counter = 0; page_counter < MAXPROCPAGES; page_counter++)
                    {
                        if (process_page != page_counter)
                        {
                            if (timestamps[process_counter][page_counter] > 0)
                            {
                                if (timestamps[process_counter][page_counter] < l)
                                {
                                    l = timestamps[process_counter][page_counter];
                                    l_i = page_counter;
                                }
                            }
                        }
                    }
                    //If pageout is able to swap out that page:
                    if (pageout(process_counter, l_i))
                    {
                        //Reset timestamp to zero
                        timestamps[process_counter][l_i] = tick;
                    }                  
                }
            }

            else
            {
                //Update timestamp to tick
                timestamps[process_counter][process_page] = tick;
            }
        }
    }


    /* advance time for next pageit iteration */
    tick++;
} 



/*

Loop all available processes

    If page is active

        Find page being requested of that active process

        Is page swapped in?

            YES: Select another process to work on

            NO: Call pagein()

                SUCCEED: Select another process to work on

                FAIL: SELECT PAGE TO EVICT

                    Call pageout()
    

Each time a page does not need to be evicted, increment timestamp for that page each iteration.

If a page must be evicted:

    L is infinity

    Loop finds page with lowest timestamp, and calls pageout() on that page
