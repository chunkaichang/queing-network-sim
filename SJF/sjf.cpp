/*
 * sjf.cpp
 * This program simulate a simplified computer system with a run queue, a wait queue,
 * and three I/O queues. The run queue adopts shortest-job-first, while all other queues
 * adopt the first-come-first-serve policy.
 * 
 * Author: 			Chun-kai Chang
 * Version: 		1
 * Creation Date:	Sun Dec 15 10:05:00 2013
 * Filename:		sjf.cpp
 * History:
 *		CK		1	Sun Dec 15 10:05:00 2013
 *				First written(based on fcfs.cpp)
 */

#include "sjf.h"
using namespace std;

#define MAX_NUM_JOB	32768

#define ARG 5
#define DISCARD_RATIO 0.05
#define MAX_CPU 4
#define RUNTIME 1000000
#define DQ_MEAN_SERVICE_TIME 200
#define EQ_MEAN_SERVICE_TIME 300
#define TQ_MEAN_SERVICE_TIME 100
#define wq_threshold 500
#define sig_ratio 0.1
#define totalLeave 1000
static int n_job; //number of current jobs
static int n_leave;//number of jobs left the system 
static int n_rq, n_wq, n_dq, n_eq, n_tq; // queue length
static int n_timeout; //number of jobs that have stayed in the wait queue for a long time
static unsigned long long cur_time;
static unsigned long long mt_sum;//sum of mean time in system
static unsigned long long rq_response_sum;//sum of response time of run queue
static unsigned long long wq_response_sum;//sum of response time of run queue
static unsigned long long dq_response_sum;//sum of response time of run queue
static unsigned long long eq_response_sum;//sum of response time of run queue
static unsigned long long tq_response_sum;//sum of response time of run queue
static unsigned long long rq_busy_sum;
static unsigned long long wq_busy_sum;
static unsigned long long dq_busy_sum;
static unsigned long long eq_busy_sum;
static unsigned long long tq_busy_sum;
static unsigned long long rq_last_event;
static unsigned long long wq_last_event;
static unsigned long long dq_last_event;
static unsigned long long eq_last_event;
static unsigned long long tq_last_event;

static int n_rq_leave,n_wq_leave,n_dq_leave,n_eq_leave,n_tq_leave;//number of jobs having left each queue

static int RQ_MEAN_SERVICE_TIME;
static int NUM_CPU;
static int JOB_ARRIVAL_MEAN_TIME;
static int rqInService;
static double LEAVE_RATIO;

priority_queue<event_t, vector<event_t>, EventCompareTime> event_list;
priority_queue<job_t, vector<job_t>, JobCompareTime> rq, wq, dq, eq, tq;

int main(int argc, char** argv)
{
	if(argc != ARG){
	    cout<<"Wrong format of arguments! [PATH][SERVICE_MEAN][ARRIVAL_MEAN][NUM_CPU][LEAVE_RATIO]"<<endl;
	    return 3;
	}
	RQ_MEAN_SERVICE_TIME = atoi(argv[1]);
	JOB_ARRIVAL_MEAN_TIME = atoi(argv[2]);
	NUM_CPU = atoi(argv[3]);
	LEAVE_RATIO = atof(argv[4]);

	cur_time = 0;
	event_t cur_event;
	//Initialize RNG
	srand(time(NULL));
	//Initialize Event List and queues
	init();
	
	//Main loop
	while(cur_time < RUNTIME){
		cur_event = event_list.top();
		cur_time = cur_event.time;

		event_list.pop();
		exe_event_handler(cur_event.type);
	}
	//Print the report
	print_report();
	
	return 0;
}

void init(){
	//Init queues
	n_rq = 0; n_wq = 0; n_dq= 0; n_eq = 0; n_tq = 0;rqInService = 0;

	//Init statistics
	n_job = 0; n_leave = 0; n_timeout = 0;
	mt_sum = 0; 
	rq_response_sum = 0; wq_response_sum = 0; dq_response_sum = 0; 
	eq_response_sum = 0; tq_response_sum = 0;
	n_rq_leave = 0;n_wq_leave = 0;n_dq_leave = 0;n_eq_leave = 0;n_tq_leave = 0;
	rq_busy_sum= 0;wq_busy_sum= 0;dq_busy_sum= 0;eq_busy_sum= 0;tq_busy_sum= 0;
	rq_last_event=0;wq_last_event=0;dq_last_event=0;eq_last_event=0;tq_last_event=0;
	
	//Add a job_arrival event into the list
	int entry_time = expo(JOB_ARRIVAL_MEAN_TIME);
	event_t evt = {JOB_ARRIVAL, entry_time};
	event_list.push(evt);
}

unsigned long long expo(int mean){
	//antithetic variable simulation is used here
	double u = (rand()%100)/100.0;
	unsigned long long t = -mean*log(u);
	unsigned long long t1 = -mean*log(1-u);
	t = (t + t1)/2;
	if(t==0)return t+1;
	else return t;
}

void exe_event_handler(int event_type){
	switch (event_type) {
		case JOB_ARRIVAL: job_arrival();break;	
		case LEAVE_RQ: rq_dequeue();break;
		case LEAVE_DQ: dq_dequeue();break;
		case LEAVE_EQ: eq_dequeue();break;
		case LEAVE_TQ: tq_dequeue();break;
		default:; 
	}
}

void job_arrival(){
	//Job initialization
	job_t job;
	job.entry_sys_time = cur_time;
	job.entry_q_time = cur_time;
	job.exe_time = expo(RQ_MEAN_SERVICE_TIME);
	job.in_service = false;
	
	//Init signal
	int s = rand()%1000;
	if(s<1000*sig_ratio)job.signal = 1;
	else job.signal = 0;

	//Check whether the max number of processes has reached
	if(n_job >= MAX_NUM_JOB) return;
	n_job++;
	
	//Enqueue the job into run queue
	rq_enqueue(job);

}

void rq_enqueue(job_t job){
	//increase the queue length
	n_rq++;
	
	//record the time on entry
	job.entry_q_time = cur_time;
	
	if(n_rq > 0 && n_rq <= NUM_CPU){//at least one CPU is spare
		sched_departure(LEAVE_RQ,job);
		job.in_service = true;//marked the job as in-service
		rqInService++;
	}	
	else //all cpu are busy		
		if(cur_time > RUNTIME*DISCARD_RATIO -1 )
		    rq_busy_sum += (cur_time - rq_last_event);
	
	//insert a job into queue	
	rq.push(job);


	//schedule next job arrival
	unsigned long long entry_time = cur_time + expo(JOB_ARRIVAL_MEAN_TIME);
	event_t evt = {JOB_ARRIVAL, entry_time};
	event_list.push(evt);
	rq_last_event = cur_time;
}

void rq_dequeue(){
	if(n_rq ==0)return;
	//decrease the queue length
	n_rq--;
	n_rq_leave++;
	rqInService--;
	
	job_t job = rq.top();
	rq.pop();

	//update rq response time 
	rq_response_sum += (cur_time - job.entry_q_time);	
				
	
	//update total busy time
	if(cur_time > RUNTIME*DISCARD_RATIO -1)
	    rq_busy_sum += (cur_time - rq_last_event);
	rq_last_event = cur_time;
			
	if(job.signal == 1) wq_dequeue();	
	
	//schedule next job leaving run queue by grabbing the first not-in-service job
	if(n_rq > NUM_CPU-1){
		job_t temp[MAX_CPU];
		int i;
		for(i=0;i<rqInService;i++){	//pop those in service
			temp[i] = rq.top();
			rq.pop();
		}
		
		job = rq.top();				//pop the first not-in-service
		job.in_service = true;
		
		for(i=0;i<rqInService;i++){	//push back those in service
			rq.push(temp[i]);
		}		
		sched_departure(LEAVE_RQ,job);
    }		
	//decide where the departure job should go
	int r = rand()%1000;
	if(r<1000*LEAVE_RATIO)leave_system(job);
	else{
	r = rand()%4;
	switch(r){
		case 0: wq_enqueue(job);break;
		case 1: dq_enqueue(job);break;
		case 2: eq_enqueue(job);break;
		case 3: tq_enqueue(job);break;
		default:;
	}
	}
}

void wq_enqueue(job_t job){
	//increase the queue length
	n_wq++;
	
	//record the time on entry
	job.entry_q_time = cur_time;
	
	//insert a job
	wq.push(job);
}

void wq_dequeue(){
	//check whether the queue is empty 
	if(n_wq == 0) return;
	
	job_t job = wq.top();
	wq.pop();
	
	//update wq response time 
	wq_response_sum += (cur_time - job.entry_q_time);
	
	//check whether the job has stayed in the system for a long time
	if(cur_time - job.entry_q_time > wq_threshold) 
		n_timeout++;
		
	n_wq--;
	n_wq_leave++;
	rq_enqueue(job);
}
void dq_enqueue(job_t job){
	//increase the queue length
	n_dq++;
	
	//record the time on entry
	job.entry_q_time = cur_time;
	
	//insert a job
	dq.push(job);
	
	//schedule next job to leave
	job_t ignore;
	if(n_dq == 1) sched_departure(LEAVE_DQ,ignore);
}

void dq_dequeue(){
	if(n_dq == 0) return;

	job_t job = dq.top();
	dq.pop();

	dq_response_sum += (cur_time - job.entry_q_time);
	
	n_dq--;
	n_dq_leave++;
	job_t ignore;
	if(n_dq > 0) sched_departure(LEAVE_DQ,ignore);
	
	rq_enqueue(job);	//put the leaving job back to rq
	wq_dequeue();		//notify wq to dequeue if there is at least one job inside
}

void eq_enqueue(job_t job){
	//increase the queue length
	n_eq++;
	
	//record the time on entry
	job.entry_q_time = cur_time;
	
	//insert a job
	eq.push(job);
	
	//schedule next job to leave
	job_t ignore;
	if(n_eq == 1) sched_departure(LEAVE_EQ,ignore);
}

void eq_dequeue(){
	if(n_eq == 0) return;

	job_t job = eq.top();
	eq.pop();
	
	eq_response_sum += (cur_time - job.entry_q_time);
	
	n_eq--;
	n_eq_leave++;
	job_t ignore;
	if(n_eq > 0) sched_departure(LEAVE_EQ,ignore);
	
	rq_enqueue(job);	//put the leaving job back to rq
	wq_dequeue();		//notify wq to dequeue if there is at least one job inside
}

void tq_enqueue(job_t job){
	//increase the queue length
	n_tq++;
	
	//record the time on entry
	job.entry_q_time = cur_time;
	
	//insert a job
	tq.push(job);
	
	//schedule next job to leave
	job_t ignore;
	if(n_tq == 1) sched_departure(LEAVE_TQ,ignore);
}

void tq_dequeue(){
	if(n_tq == 0) return;

	job_t job = tq.top();
	tq.pop();
	
	tq_response_sum += (cur_time - job.entry_q_time);
	
	n_tq--;
	n_tq_leave++;
	job_t ignore;
	if(n_tq > 0) sched_departure(LEAVE_TQ,ignore);
	
	rq_enqueue(job);	//put the leaving job back to rq
	wq_dequeue();		//notify wq to dequeue if there is at least one job inside
}

void leave_system(job_t job){
	n_job--;
	//update the sum of mean time in system
	//to eliminate initialization bias, some values should be discarded
	//here, I discard the first 5% 
	if(cur_time > RUNTIME*DISCARD_RATIO - 1){
	    n_leave++;
	    mt_sum += (cur_time - job.entry_sys_time);
	}
	
}

void sched_departure(int type,job_t job){
	unsigned long long depart_time;
	event_t evt;
	
	switch(type){
		case LEAVE_RQ: {
			depart_time = cur_time + job.exe_time;
			evt.type = LEAVE_RQ; 
			evt.time = depart_time;			
			break;
		}	
		case LEAVE_DQ: {
			depart_time = cur_time + expo(DQ_MEAN_SERVICE_TIME);
			evt.type = LEAVE_DQ;
			evt.time = depart_time;
			break;
		}	
		case LEAVE_EQ: {
			depart_time = cur_time + expo(EQ_MEAN_SERVICE_TIME);
			evt.type = LEAVE_EQ;
			evt.time = depart_time;
			break;
		}
		case LEAVE_TQ: {
			depart_time = cur_time + expo(TQ_MEAN_SERVICE_TIME); 
			evt.type = LEAVE_TQ;
			evt.time = depart_time;
			break;
		}	
		default:;
	}	

	event_list.push(evt);

}


void print_report(){
	
	//mean time in system
	double mean_time = mt_sum / (double)(n_leave);
	//average response time
	double rq_avgr = rq_response_sum / (double)n_rq_leave;

	double  wq_avgr = 0, dq_avgr = 0, eq_avgr = 0, tq_avgr = 0;
	if(n_wq_leave != 0)
		wq_avgr = wq_response_sum / (double)n_wq_leave;
	if(n_dq_leave != 0) 
		dq_avgr = dq_response_sum / (double)n_dq_leave;
	if(n_eq_leave != 0) 
		eq_avgr = eq_response_sum / (double)n_eq_leave;		
	if(n_tq_leave != 0) 
		tq_avgr = tq_response_sum / (double)n_tq_leave;
	//utilization of each queue
	double u_rq = rq_busy_sum / (double)RUNTIME*(1-DISCARD_RATIO);
		
	cout << "Cur_time "<<cur_time<<endl;
	cout << "Mean time in the system             " << setw(10) << mean_time << endl;
	cout << "Run queue utilization               " << setw(10) << u_rq << endl;	
	cout << "Run queue average response time     " << setw(10) << rq_avgr << endl;
	cout << "Wait queue average response time    " << setw(10) << wq_avgr << endl;
	cout << "Disk queue average response time    " << setw(10) << dq_avgr << endl;
	cout << "Etherent queue average response time" << setw(10) << eq_avgr << endl;
	cout << "Terminal queue average response time" << setw(10) << tq_avgr << endl;	
	
}
