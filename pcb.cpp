#pragma once
#include <iostream>
#include <vector>
using namespace std;

class PCB {
    private:
        int PID; // ranging from PIDMIN to PIDMAX specified in simulation.cpp
        int PRIORITY; // 1 - high, 2 - medium, 3 - low
        int STATE; // 1 - ready, 2 - waiting (for IO), 3 - running, 4 - finished
        vector<int> IO_TIME; // in ms
        vector<int> CPU_TIME; // in ms
        int TURNAROUND; // in ms, for analysis purposes
        int WAIT; // in ms, for analysis purposes
        int RESPONSE; // in ms, for analysis purposes

    public:
        // empty constructor, used when initializing processes
        PCB() : PID(-1), PRIORITY(-1), STATE(-1), IO_TIME({-1}), CPU_TIME({-1}){};
        // specific constructor, used when reading from generated process file
        PCB(int pid, int pri, int sta, vector<int> io, vector<int> cpu, int thr) : PID(pid), PRIORITY(pri), STATE(sta), IO_TIME(io), CPU_TIME(cpu){};

        // getters for each PCB attribute
        int getPID() {return PID;}
        int getPRIORITY() {return PRIORITY;}
        int getSTATE() {return STATE;}
        vector<int> getIO_TIME() {return IO_TIME;}
        vector<int> getCPU_TIME() {return CPU_TIME;}
        int getTURNAROUND() { return TURNAROUND; }
        int getWAIT() { return WAIT; }
        int getRESPONSE() { return RESPONSE; }

        // setters for each PCB attribute
        void setPID(int pid) { PID = pid; }
        void setPRIORITY(int pri) { PRIORITY = pri; }
        void setSTATE(int sta) { STATE = sta; }
        void setIO_TIME(vector<int> iot) { IO_TIME = iot; }
        void setCPU_TIME(vector<int> cpt) { CPU_TIME = cpt; }
        void setTURNAROUND(int tur) { TURNAROUND = tur; }
        void setWAIT(int wait) { WAIT = wait; }
        void setRESPONSE(int res) { RESPONSE = res; }

        // overloaded output operator.  mostly used for debugging purposes
        friend ostream & operator<<(ostream & ostr, PCB * pcb) {
            ostr << pcb->getPID() << ", " << pcb->getPRIORITY() << ", " << pcb->getSTATE() << ", "; 
            for (int i = 0; i < pcb->getIO_TIME().size(); i++) {
                ostr << pcb->getIO_TIME()[i] << ", ";
            }
            for (int i = 0; i < pcb->getCPU_TIME().size(); i++) {
                ostr << pcb->getCPU_TIME()[i] << ", ";
            }
            return ostr;
        }
};