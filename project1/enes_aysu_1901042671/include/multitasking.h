 
#ifndef __MYOS__MULTITASKING_H
#define __MYOS__MULTITASKING_H

#include <common/types.h>
#include <gdt.h>

namespace myos
{
    struct CPUState
    {
        common::uint32_t eax; // a register
        common::uint32_t ebx; // b register
        common::uint32_t ecx; // c register
        common::uint32_t edx; // d register

        common::uint32_t esi;
        common::uint32_t edi;
        common::uint32_t ebp;

        /*
        common::uint32_t gs;
        common::uint32_t fs;
        common::uint32_t es;
        common::uint32_t ds;
        */
        common::uint32_t error;

        common::uint32_t eip; // instruction pointer register
        common::uint32_t cs;  // call segment
        common::uint32_t eflags;
        common::uint32_t esp; // stack pointer register
        common::uint32_t ss;        
    } __attribute__((packed));
    
    
    class Task
    {
        friend class TaskManager;
        private:
            static common::uint32_t pIdCounter;
            common::uint8_t stack[4096]; // 4 KiB
            common::uint32_t pid = 0;
            common::uint32_t pPid = 0; // parent pid
            common::uint32_t cPid = 0; //child pid
            common::uint8_t taskState = 0; // 0 ---> waiting state , 1 ---> ready state, 2 ---> running state
            common::uint32_t waitPid;
            CPUState* cpustate;

        public:
            Task(GlobalDescriptorTable *gdt, void entrypoint());
            ~Task();
            Task();
            common::uint32_t getId();
    };

    class TaskManager
    {
        private:
            Task tasks[256];
            int numTasks;
            int currentTask;

        public:
            //void PrintProcessTable();
            common::uint32_t ForkTask(CPUState *cpustate);
            common::uint32_t ExecTask(void entrypoint());
            common::uint32_t AddTask(void entrypoint());
            common::uint32_t GetPID();
            common::uint32_t GetCPID();
            void taskTable();
            int getIndex(common::uint32_t pid);
            bool ExitTask();
            bool WaitTask(common::uint32_t esp);
            bool ExitCurrentTask();
            
            TaskManager();
            ~TaskManager();
            bool AddTask(Task *task);
            CPUState* Schedule(CPUState* cpustate);
    };
}
#endif