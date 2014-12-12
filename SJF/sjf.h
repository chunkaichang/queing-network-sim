/*
 * sjf.h - contains data structures for the event list and queues
 *
 * 
 * Author: 			Chun-kai Chang
 * Version: 		1
 * Creation Date:	Sun Dec 15 10:01:11 2013
 * Filename:		sjf.h
 * History:
 *		CK		1	Sun Dec 15 10:01:11 2013
 *				First written(based on fcfs.h)
 */
#ifndef SJF_H
#define SJF_H

#include <iostream>
#include <queue>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <iomanip>

//////////////////
//* Event List *//
//////////////////
enum event_type {JOB_ARRIVAL, LEAVE_RQ, LEAVE_DQ, LEAVE_EQ, LEAVE_TQ};

typedef struct event{
	event_type type;
	unsigned long long time;		//time to trigger the event
} event_t;

class EventCompareTime {
public:
	bool operator()(event_t &e1, event_t &e2)
	{
		if(e1.time > e2.time) return true;
		else return false;
	}
};	
////////////
//* Jobs *//
////////////
typedef struct job{
	unsigned long long entry_sys_time;	//time entering the system
	unsigned long long entry_q_time;	//time entering the one of the queue
	int signal;							//carry a signal or not
	unsigned long long exe_time;		//execution time in cpu(for shortest-job first)
	bool in_service;
} job_t;

class JobCompareTime {
public:
	bool operator()(job_t &j1, job_t &j2)
	{
		if(!j1.in_service && !j2.in_service){//both not in service
			if(j1.exe_time > j2.exe_time) return true;
			else return false;
		}
		else if(j1.in_service && j2.in_service)//both in service
			return (j1.entry_q_time+j1.exe_time > j2.entry_q_time+j2.exe_time);
		else if(j1.in_service)
			return false;
		else
			return true;
	}
};
//Initialize Event List and all queues	
extern void init();

//Exponential RNG
extern unsigned long long  expo(int mean);

//Execute the event handler
extern void exe_event_handler(int event_type);

//Print the report
extern void print_report();

extern void job_arrival();
extern void rq_enqueue(job_t job);
extern void rq_dequeue();
extern void wq_enqueue(job_t job);
extern void wq_dequeue();
extern void dq_enqueue(job_t job);
extern void dq_dequeue();
extern void eq_enqueue(job_t job);
extern void eq_dequeue();
extern void tq_enqueue(job_t job);
extern void tq_dequeue();
extern void leave_system(job_t job);
extern void sched_departure(int type,job_t job);


#endif /* SJF_H */

