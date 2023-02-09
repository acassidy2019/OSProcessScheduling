#include "pcb.cpp"
#include <fstream>
#include <time.h>
#include <string>
#include <queue>
#include <iomanip>

// tons of constants: mostly mins and maxes
#define CORECOUNT 16
#define HIGHRES CORECOUNT / 2 // reserved core amount for high priority processes
#define MEDRES CORECOUNT / 3 // reserved core amount for medium priority processes
#define LOWRES HIGHRES - MEDRES // reserved core amount for low priority processes
#define PROCMIN 50 // min number of processes per sim
#define PROCMAX 100 // max number of processes per sim
#define PIDMIN 30 // min pid value
#define PIDMAX PIDMIN + (PROCMAX - PROCMIN) // max pid: defined by the minpid + (range of process count)
#define BURSTMIN 1 // min number of bursts per process
#define BURSTMAX 8 // max number of bursts per process
#define CPUMIN 30 // min cpu_burst size in ms
#define CPUMAX 60 // max cpu_burst size in ms
#define IOMIN 5 // min io_burst size in ms
#define IOMAX 10 // max io_burst size in ms
#define RRTIME 40 // must be less than CPUMAX in order for RR to be effective and not default to pure FCFS
#define RUNCOUNT 100 // number of times to run the simulation
#define PRINTMODE false // used for whether or not to print outeach sim run's results

// define gen_processes function (found at the bottom)
void gen_processes(vector<vector<string>>& proc_t, vector<PCB *>& proc_b);

///////////////////////////////////////////////////
///// MAIN SIMULATION /////////////////////////////
///////////////////////////////////////////////////

// main function for this sim program
int main() {
    srand(time(NULL));

    // values for overall averaged analytics 
    int simruntimes[RUNCOUNT]; // sim run times
    double throughputs[RUNCOUNT];
    int turnaround_averages[RUNCOUNT]; // stacking of average turnaround times
    int wait_averages[RUNCOUNT]; // stacking of average wait times
    int response_averages[RUNCOUNT]; // stacking of average response times 
    int core_idle_times[RUNCOUNT]; // stacking of cit: core idle time is the total idle time accumulated over all cores
    int cpu_idle_times[RUNCOUNT]; // stacking of cpuit: cpu idle time is the total time a core was idle 

    ofstream output; // output to file
    output.open("results.txt");
    
    int simrun = 0;
    while (simrun < RUNCOUNT) {
        // different process queues based on priority
        queue<int> high_queue; // high priority processes get assigned to half of the available processors
        queue<int> med_queue; // medium priority processes get assigned to a third of the available processors
        queue<int> low_queue; // low priority processes get assigned to the remaining processors
        queue<int> wait_queue; // queue for processes waiting on an io burst   

        // PCB vector
        vector<PCB *> proc_blocks;

        // Process table
        // in each row: pid, arrival time, assigned processor, finish time
        vector<vector<string>> proc_table; // outer is processes, inner is process attributes

        // core array, holds the PID of the process it is currently handling
        // if -1, then no PID currently assigned
        int cores[CORECOUNT];
        for (int i = 0; i < CORECOUNT; i++) { cores[i] = -1; }
        
        // generate random bursts of processes in a .txt file and parse into PCB objects
        // see above
        gen_processes(proc_table, proc_blocks);

        // initialize processes into their respectful queues and initialize analytic times
        for (int i = 0; i < proc_blocks.size(); i++) {
            if (proc_blocks[i]->getPRIORITY() == 1) {
                high_queue.push(proc_blocks[i]->getPID()); // if a high priority process, add to high priority queue
            } else if (proc_blocks[i]->getPRIORITY() == 2) {
                med_queue.push(proc_blocks[i]->getPID()); // if a medium priority process, add to high priority queue
            } else if (proc_blocks[i]->getPRIORITY() == 3) {
                low_queue.push(proc_blocks[i]->getPID()); // if a low priority process, add to high priority queue
            } else {
                // should not be reached, would mean a process was given an invalid priority
                cerr << "Invalid process priority.\n";
            }
            proc_blocks[i]->setTURNAROUND(0);
            proc_blocks[i]->setWAIT(0);
            proc_blocks[i]->setRESPONSE(0);
        }

        //cout << "processes initialized\n";

        // values for analytics per sim
        int sim_simruntime = 0;
        double sim_throughput = 0.0;
        int sim_core_idle = 0; // core idle time is the total idle time accumulated over all cores
        int sim_cpu_idle = 0; // cpu idle time is the total time the cpu was in an idle state (a core was idle)
        int cpu_bursts = 0; // keep track of number of cpu bursts worked through

        ///// MAIN PROCESS HANDLER /////

        bool run = true;
        while (run) {
            // add round robin time to every process's turnaround
            for (int i = 0; i < proc_blocks.size(); i++) {
                if (proc_blocks[i]->getSTATE() != 4)    
                    proc_blocks[i]->setTURNAROUND(proc_blocks[i]->getTURNAROUND() + RRTIME);
            }
            //cout << "turnarounds updated.\n";
            // increment simruntime time by round robin value when no process was a finished process
            sim_simruntime += RRTIME;

            // used for calculating time of last process and adding to analysis values 
            int final_cpu_time;

            ///// CURRENT PROCESSES /////

            bool idle = false; // if a core was idle, only increment cpu idle time once
            // check each process currently in a processor
            for (int i = 0; i < CORECOUNT; i++) {
                if (cores[i] > 0) { // if there is a process assigned to this core
                    // subtract round robin time, and see if process has finished.  
                    // if so,  add CPU_TIME to PCB simruntime, remove burst from that vector
                    // otherwise, move back into its queue type
                    vector<int> cputimes = proc_blocks[cores[i] - PIDMIN]->getCPU_TIME();
                    if (cputimes.empty()) break;

                    int time = cputimes[0] - RRTIME;
                    if (time <= 0) { // if process is complete
                        // update the turnaround based on the fact that this process is complete and may not have used 
                        // the full round robin time quantum
                        proc_blocks[cores[i] - PIDMIN]->setTURNAROUND(proc_blocks[cores[i] - PIDMIN]->getTURNAROUND() + cputimes[0]); 
                        // erase that process's cpu burst
                        cputimes.erase(cputimes.begin()); 
                        // check if process has an IO burst next
                        if (!proc_blocks[cores[i] - PIDMIN]->getIO_TIME().empty()) {
                            wait_queue.push(cores[i]); // add this process to the wait queue
                            proc_blocks[cores[i] - PIDMIN]->setSTATE(2); // update process state to "waiting"
                        } else { // if process is complete (no need to check cpu_bursts, since it will not have two in a row)
                            proc_blocks[cores[i] - PIDMIN]->setSTATE(4); // update process state to "finished"
                        }    
                            
                        // reset the PCB's cpu_time vector now that one burst is removed
                        proc_blocks[cores[i] - PIDMIN]->setCPU_TIME(cputimes);
                        // set final cpu_time val in case it is needed
                        final_cpu_time = time + RRTIME;
                        cpu_bursts++;
                    } else { // if process is not complete
                        // subtract necessary cpu times
                        cputimes[0] = time;
                        proc_blocks[cores[i] - PIDMIN]->setCPU_TIME(cputimes);
                        // move process back into its queue based on priority type
                        if (proc_blocks[cores[i] - PIDMIN]->getPRIORITY() == 1) {
                            high_queue.push(cores[i]);
                        } else if (proc_blocks[cores[i] - PIDMIN]->getPRIORITY() == 2) {
                            med_queue.push(cores[i]);
                        } else if (proc_blocks[cores[i] - PIDMIN]->getPRIORITY() == 3) {
                            low_queue.push(cores[i]);
                        } 
                        // if priority is not one of these options, then it either was never assigned a priority or it is finished
                        // should not reach this part of the code, but if it does throw an error
                        else cerr << "Process priority mishandle.\n";
                    }
                } else { // if core not being used,  add idle time
                    if (!idle) {    
                        sim_cpu_idle += RRTIME;
                        idle = true;                    
                    }
                    sim_core_idle += RRTIME;
                }
                // reset core to "available" flag
                // all cores should be reset to available each cycle
                cores[i] = -1;
            }
            
            // cout << "current processes dealt with\n";

            // increment the time spent waiting in a queue, adding to the overall response time
            for (int i = 0; i < proc_blocks.size(); i++) {
                if (proc_blocks[i]->getSTATE() == 1) // if waiting in a queue, add round robin time to response time
                    proc_blocks[i]->setRESPONSE(proc_blocks[i]->getRESPONSE() + RRTIME);
            }

            ///// LOADING IN NEW PROCESSES /////
            
            // assign processors to new processes
            // each of these are based on a FCFS within each queue
            // assign half of the processors to high priority processes
            for (int i = 0; i < HIGHRES; i++) {
                if (!high_queue.empty()) {    
                    cores[i] = high_queue.front(); // set the core to the first available process in the queue
                    high_queue.pop(); // remove that process from the process queue
                    proc_blocks[cores[i] - PIDMIN]->setSTATE(3); // update process state to "running"
                }
            } 

            // assign one third of the processors to medium priority processes
            for (int i = HIGHRES; i < HIGHRES + MEDRES; i++) {
                if (!med_queue.empty()) {    
                    cores[i] = med_queue.front(); // set the core to the first available process in the queue
                    med_queue.pop(); // remove that process from the process queue
                    proc_blocks[cores[i] - PIDMIN]->setSTATE(3); // update process state to "running"
                }
            }

            // assign remaining processors to low priority processes
            for (int i = HIGHRES + MEDRES; i < CORECOUNT; i++) {
                if (!low_queue.empty()) {    
                    cores[i] = low_queue.front(); // set the core to the first available process in the queue
                    low_queue.pop(); // remove that process from the process queue
                    proc_blocks[cores[i] - PIDMIN]->setSTATE(3); // update process state to "running"
                }
            }
            
            // after each queue has had a chance to get a core assigned, assign any remaining available processors 
            // to the next available process in high queue, then med queue, then low queue
            for (int i = 0; i < CORECOUNT; i++) {
                if (cores[i] == -1) {
                    if (!high_queue.empty()) {    
                        cores[i] = high_queue.front(); // set the core to the first available process in the queue
                        high_queue.pop(); // remove that process from the process queue
                        proc_blocks[cores[i] - PIDMIN]->setSTATE(3); // update process state to "running"
                    } else if (!med_queue.empty()) {    
                        cores[i] = med_queue.front(); // set the core to the first available process in the queue
                        med_queue.pop(); // remove that process from the process queue
                        proc_blocks[cores[i] - PIDMIN]->setSTATE(3); // update process state to "running"
                    } else if (!low_queue.empty()) {    
                        cores[i] = low_queue.front(); // set the core to the first available process in the queue
                        low_queue.pop(); // remove that process from the process queue
                        proc_blocks[cores[i] - PIDMIN]->setSTATE(3); // update process state to "running"
                    } 
                    // if all queues empty, core stays empty and idle time will be added in next cycle (above)
                } 
            }

            // implement SRT algorithm
            // if a process has a remaining time less than the Round Robin time, add it to high priority
            
            //cout << "new processes loaded in\n";

            ///// HANDLING IO BURSTS /////
            
            // check next IO burst up in the queue. if round robin time is sufficient, then pass back to cpu queues if applicable
            // and properly update wait time and turnaround
            // this is organized so that every round robin cycle, we will get exactly RRTIME's worth of io_burst time worked through
            int totio = 0;
            bool io_processing = true;
            int final_io_time; // used in similar fashion to final_cpu_time, only meant for if last io_burst
            while( io_processing ) {
                if (!wait_queue.empty()) {
                    // add next io burst to io time processed this round
                    vector<int> iotimes = proc_blocks[wait_queue.front() - PIDMIN]->getIO_TIME();
                    //cout << proc_blocks[wait_queue.front() - PIDMIN] << endl;
                    int io_burst = iotimes[0];
                    //if (io_burst != -1) { // for handling weird error where an io burst is -1
                        totio += io_burst;
                        int iorrdiff = RRTIME - totio;
                        if (iorrdiff < 0) { // if we have used up all round robin time in io bursts (i.e. this process did not fulfill its io burst)
                            // update process io_time to subtract the difference
                            iotimes[0] = io_burst + iorrdiff;
                            proc_blocks[wait_queue.front() - PIDMIN]->setIO_TIME(iotimes);
                            // add to the wait time for all processes in the queue
                            for (int i = 0; i < proc_blocks.size(); i ++) {
                                if (proc_blocks[i]->getSTATE() == 2) // if this process is in the wait queue, add to its wait time
                                    proc_blocks[i]->setWAIT(proc_blocks[i]->getWAIT() + io_burst);
                            }

                            io_processing = false;
                            
                            // update final_io_time for future reference based on final burst times
                            final_io_time = io_burst;
                        } else { // if we have not used up all of the wait time (i.e. this process fulfilled its io burst)
                            if (iorrdiff == 0) io_processing = false; // if used up, pop process off and exit
                            // add wait time to the PCB for all processes in queue
                            for (int i = 0; i < proc_blocks.size(); i ++) {
                                if (proc_blocks[i]->getSTATE() == 2)  // if this process is in the wait queue, add to its wait time
                                    proc_blocks[i]->setWAIT(proc_blocks[i]->getWAIT() + io_burst);
                            }

                            // remove the used io_burst from the PCB
                            iotimes.erase(iotimes.begin());
                            proc_blocks[wait_queue.front() - PIDMIN]->setIO_TIME(iotimes);

                            // if process has another cpu burst, send back to its queue
                            if (!proc_blocks[wait_queue.front() - PIDMIN]->getCPU_TIME().empty()) {
                                // send process back to its respective queue
                                if (proc_blocks[wait_queue.front() - PIDMIN]->getPRIORITY() == 1) {
                                    high_queue.push(wait_queue.front());
                                    proc_blocks[high_queue.back() - PIDMIN]->setSTATE(1); // update state to ready
                                } else if (proc_blocks[wait_queue.front() - PIDMIN]->getPRIORITY() == 2) {
                                    med_queue.push(wait_queue.front());
                                    proc_blocks[med_queue.back() - PIDMIN]->setSTATE(1); // update state to ready
                                } else { // guaranteed to be a low-priority process, as any other value would not have made it his far
                                    low_queue.push(wait_queue.front());
                                    proc_blocks[low_queue.back() - PIDMIN]->setSTATE(1); // update state to ready
                                }
                            } else { // if no remaining cpu bursts, process is then finished (no need to check io bursts, since it will not have two in a row)
                                proc_blocks[wait_queue.front() - PIDMIN]->setSTATE(4); // update process state to "finished"
                            }
                            wait_queue.pop();
                        } 
                    //} else wait_queue.pop();
                } else io_processing = false;
            }

            //cout << "io bursts handled\n";

            // if all queues are empty, quit
            if (high_queue.empty() &&
                med_queue.empty() &&
                low_queue.empty() &&
                wait_queue.empty())  {
                    run = false;
                    //cout << "Simulation Run Complete." << endl;
                    // update with final burst time splits
                    sim_simruntime += final_cpu_time += final_io_time;
                }
        }
        // calculate throughput of this simulation
        sim_throughput = static_cast<double>(proc_blocks.size()) / static_cast<double>(sim_simruntime);
        
        // load turnaround, wait, and response times into analysis variables
        int tot_turnaround = 0;
        int tot_wait = 0;
        int tot_response = 0;
        
        for (int i = 0; i < proc_blocks.size(); i++) {
            tot_turnaround += proc_blocks[i]->getTURNAROUND();
            tot_wait += proc_blocks[i]->getWAIT();
            tot_response += proc_blocks[i]->getRESPONSE();
        }

        // load calculated analytics into our outer containers for averaging
        simruntimes[simrun] = sim_simruntime;
        throughputs[simrun] = sim_throughput;
        turnaround_averages[simrun] = tot_turnaround / proc_blocks.size();
        response_averages[simrun] = tot_response / cpu_bursts;
        wait_averages[simrun] = tot_wait / proc_blocks.size();
        core_idle_times[simrun] = sim_core_idle;
        cpu_idle_times[simrun] = sim_cpu_idle;

        if (PRINTMODE) { 
            // print and compile analytics from this simulation run    
            // update simruntime information
            output << "Simulation " << simrun + 1 << ": ";
            output << "Sim run time: " << simruntimes[simrun] << endl;

            // print throughput information
            output << "Simulation " << simrun + 1 << ": ";
            output << "Average throughput: " << throughputs[simrun] << endl;
            
            // update turnaround information
            output << "Simulation " << simrun + 1 << ": ";
            output << "Average turnaround: " << turnaround_averages[simrun] << endl;

            // update wait time information
            output << "Simulation " << simrun + 1 << ": ";
            output << "Average wait time: " << wait_averages[simrun] << endl;

            // update response time information
            output << "Simulation " << simrun + 1 << ": ";
            output << "Average response time: " << response_averages[simrun] << endl;

            // update cpu idle time information
            output << "Simulation " << simrun + 1 << ": ";
            output << "Total Core Idle Time: " << sim_core_idle  << endl;
            output << "Simulation " << simrun + 1 << ": ";
            output << "Total CPU Idle Time: " << sim_cpu_idle  << endl;
            output << endl; 
        }
        // deallocate the PCBs
        for (int i = 0; i < proc_blocks.size(); i++) {
            delete proc_blocks[i];
        }
        simrun++;
    }

    // using our averages, show average over x runs
    // add up and average the througputs and average statistics
    int srttot = 0;
    double thrtot = 0.0;
    int turavgtot = 0;
    int waiavgtot = 0;
    int resavgtot = 0;
    int coridltot = 0;
    int cpuidltot = 0;
    for (int i = 0; i < RUNCOUNT; i++)  {
        srttot += simruntimes[i];
        thrtot += throughputs[i];
        turavgtot += turnaround_averages[i];
        waiavgtot += wait_averages[i];
        resavgtot += response_averages[i];
        coridltot += core_idle_times[i];
        cpuidltot += cpu_idle_times[i];
    }
    
    // displa info gathered
    output << " -- Over " << RUNCOUNT << " runs -- " << endl;
    output << "Average sim run time: " << srttot / RUNCOUNT << endl;
    output << "Average throughput (processes/ms): " << std::setprecision(2) << thrtot / RUNCOUNT << endl;
    output << "Average turnaround time: " << turavgtot / RUNCOUNT << endl;
    output << "Average wait time: " << waiavgtot / RUNCOUNT << endl;
    output << "Average response time: " << resavgtot / RUNCOUNT << endl;
    output << "Average core idle time: " << coridltot / RUNCOUNT << endl;
    output << "Average cpu idle time: " << cpuidltot / RUNCOUNT << endl;

    output.close();

    return 0;
}

///////////////////////////////////////////////////
///// RANDOM PROCESS GENERATION AND LOADING ///////
///////////////////////////////////////////////////

// function for generating a file of random processes with bursts as well as
// loading processes into their respective PCBs based on the processes file
// key for this file is: PID, Arrival Time, Priority, CPU Burst, IO Burst, CPU Burst, IO Burst, etc
void gen_processes(vector<vector<string>>& proc_t, vector<PCB *>& proc_b) {
    
    ///// GENERATE PROCESSES INTO A FILE /////
    
    // create a random number of processes
    int num_processes = rand() % (PROCMAX - PROCMIN) + PROCMIN;
    // create file for process info to be generated in
    ofstream process_file;
    try {    
        process_file.open("processes.txt");
    } catch(...) { cerr << "Error opening processes file.\n"; }

    // fill in each line of the processes file with unique PIDs and random amounts of CPU / IO bursts
    try {
        for (int i = 0; i < num_processes; i++) {
            process_file << i + PIDMIN << ", " << i << ", ";
            // generate a random priority for this process
            process_file << rand() % 3 + 1 << ", ";
            // generate a random number of cpu / io bursts
            int bursts = rand() % (BURSTMAX - BURSTMIN) + BURSTMIN;
            for (int j = 0; j < bursts; j++) {
                // if this will be an io burst
                // generate random amount of time for the io burst
                if (j%2) process_file << rand() % (IOMAX - IOMIN) + IOMIN << ", ";
                // if this will be a cpu burst
                else process_file << rand() % (CPUMAX - CPUMIN) + CPUMIN << ", ";
            }
            if (i < num_processes - 1) process_file << endl; // remove empty line at the end
            // prepare the proc_table for process information to be added later
            proc_t.push_back({""});
        }
    } catch(...) { cerr << "Error initializing random processes in process file.\n"; }
    process_file.close();

    ///// LOAD PROCESSES INTO PCBS /////

    // open the generated processes file
    ifstream load_pcb;
    // load each process into a new PCB
    try {
        load_pcb.open("processes.txt");
        if (load_pcb.is_open()) {
            string curline;
            // read each line and load information into a process object in proc_blocks;
            int numproc = 0;
            while (getline(load_pcb, curline)) { // loop through each process
                numproc++;
                int argcount = 0;
                PCB * temp = new PCB();
                vector<string> table_vals;
                while (curline.length() > 1) { // loop through each argument
                    string arg;
                    while (curline[0] != ',') { // loop through each character
                        arg += curline[0];
                        curline.erase(curline.begin());
                    }
                    argcount++;
                    // see which property of the process was just read and set it
                    if (argcount == 1) { // if a pid
                        temp->setPID(stoi(arg));
                        table_vals.push_back(arg);
                    }
                    else if (argcount == 2) { // if the arrival time
                        table_vals.push_back(arg);
                    }
                    else if(argcount == 3) { // if the process priority
                        temp->setPRIORITY(stoi(arg));
                    }
                    else if (argcount %2) { // if an io burst
                        vector<int> curio = temp->getIO_TIME();
                        // if this is the first argmuent being entered
                        if (curio[0] == -1) curio.erase(curio.begin());
                        curio.push_back(stoi(arg));
                        temp->setIO_TIME(curio);
                    }
                    else if (argcount != 0) { // if a cpu burst
                        vector<int> curcp = temp->getCPU_TIME();
                        if (curcp[0] == -1) curcp.erase(curcp.begin());
                        curcp.push_back(stoi(arg));
                        temp->setCPU_TIME(curcp);
                    }
                    else { cout << "Empty line found in process file.\n"; break; } // empty line
                    curline.erase(curline.begin(), curline.begin()+1); // erase the ", " from the current line being read
                }
                // once process is set, load process into proc_blocks and process info into process table
                temp->setSTATE(1);
                proc_b.push_back(temp);
                // fill in table_vals with other empty strings for later use: finish time and assigned cpu
                table_vals.push_back({""});
                table_vals.push_back({""});
                proc_t[temp->getPID() - PIDMIN] = table_vals;
            }    
        } else { cerr << "Error opening generated process file for PCB loading."; }
    } catch(...) { cerr << "Error creating PCBs from generated process file."; }
    load_pcb.close();
    //cout << "Successfully loaded into PCBs\n";
}

