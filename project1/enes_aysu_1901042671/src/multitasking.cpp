
#include <multitasking.h>

using namespace myos;
using namespace myos::common;

myos::common::uint32_t myos::Task::pIdCounter = 0;

void printfHex(uint8_t key);
void printf(char*);

// STATE NUMBERS
// finished 0
// waiting  1
// ready    2
// running  3

Task::Task(GlobalDescriptorTable *gdt, void entrypoint())
{
    cpustate = (CPUState*)(stack + 4096 - sizeof(CPUState));
    
    cpustate -> eax = 0;
    cpustate -> ebx = 0;
    cpustate -> ecx = 0;
    cpustate -> edx = 0;

    cpustate -> esi = 0;
    cpustate -> edi = 0;
    cpustate -> ebp = 0;
    
    /*
    cpustate -> gs = 0;
    cpustate -> fs = 0;
    cpustate -> es = 0;
    cpustate -> ds = 0;
    */
    
    // cpustate -> error = 0;    
   
    // cpustate -> esp = ;
    cpustate -> eip = (uint32_t)entrypoint;
    cpustate -> cs = gdt->CodeSegmentSelector();
    // cpustate -> ss = ;
    cpustate -> eflags = 0x202;
    
}

Task::~Task()
{
}

Task::Task()
{
}

common::uint32_t Task::getId()
{
    return pid;
}


TaskManager::TaskManager()
{
    numTasks = 0;
    currentTask = -1; 
}

TaskManager::~TaskManager()
{
}

common::uint32_t TaskManager::ForkTask(CPUState *cpustate)
{
    if(numTasks >= 256) {
        return -1;
    }    

    tasks[numTasks].taskState = 2;
    tasks[numTasks].pPid = tasks[currentTask].pid;
    tasks[numTasks].pid = ++Task::pIdCounter;
    tasks[currentTask].cPid = tasks[numTasks].pid;

    // copying current stack to new stack
    for (int i = 0; i < sizeof(tasks[currentTask].stack); i++)
    {
        tasks[numTasks].stack[i] = tasks[currentTask].stack[i];
    }

    // finding current task's length
    common::uint32_t currentTaskOffset = (((common::uint32_t)cpustate) - ((common::uint32_t)tasks[currentTask].stack));
    // make two stacks pointer's location same
    tasks[numTasks].cpustate = (CPUState*)(tasks[numTasks].stack + currentTaskOffset);

    // child returns 0
    tasks[numTasks].cpustate -> eax = 0;

    // new task created
    numTasks++;

    return 0;
}

common::uint32_t TaskManager::GetPID() {
    // returns parent pid
    return tasks[currentTask].pid; 
}

common::uint32_t TaskManager::GetCPID() {
    // returns child pid
    return tasks[currentTask].cPid; 
}

int TaskManager::getIndex(common::uint32_t pid)
{
    // returns waiting process index
    int index = -1;
    for (int i = 0; i < numTasks; i++)
    {
        if(tasks[i].pid == pid)
        {
            index=i;
            break;

        }
    }
    return index;
}

bool TaskManager::ExitTask() {
    // set current task state finished (sys exit)
    tasks[currentTask].taskState=0;
    return true;
}

bool TaskManager::WaitTask(common::uint32_t pid) {
    // saving waiting process pid
    tasks[currentTask].taskState = 1;
    tasks[currentTask].waitPid = pid;
    return true;
}

bool TaskManager::AddTask(Task* task) {
    if(numTasks >= 256) {
        return false;
    }   

    tasks[numTasks].taskState = 2;
    tasks[numTasks].pid = ++Task::pIdCounter;
    tasks[numTasks].cpustate = (CPUState*)(tasks[numTasks].stack + 4096 - sizeof(CPUState));
    
    tasks[numTasks].cpustate -> eax = task->cpustate->eax;
    tasks[numTasks].cpustate -> ebx = task->cpustate->ebx; 
    tasks[numTasks].cpustate -> ecx = task->cpustate->ecx;
    tasks[numTasks].cpustate -> edx = task->cpustate->edx;

    tasks[numTasks].cpustate -> esi = task->cpustate->esi;
    tasks[numTasks].cpustate -> edi = task->cpustate->edi;
    tasks[numTasks].cpustate -> ebp = task->cpustate->ebp;

    tasks[numTasks].cpustate -> eip = task->cpustate->eip;
    tasks[numTasks].cpustate -> cs = task->cpustate->cs;
    tasks[numTasks].cpustate -> eflags = task->cpustate->eflags;
    tasks[numTasks].cpustate -> esp = task->cpustate->esp;
    //tasks[numTasks].cpustate -> ss = task->cpustate->ss;

    numTasks++;
    return true;
}

void TaskManager::taskTable(){
    printf("\n-----------------------------\n");
    printf("PID  PPID STATE\n");
    for (int i = 0; i < numTasks; i++)
    {
        printfHex(tasks[i].pid);
        printf("   ");
        printfHex(tasks[i].pPid);
        printf("   ");
        if(tasks[i].taskState== 2){
            if(i==currentTask)
                printf("RUNNING");
            else
                printf("READY");
        }else if(tasks[i].taskState==1){
            printf("WAITING");
        }else if(tasks[i].taskState==0){
            printf("FINISHED");
        }
        printf("\n");
    }
    printf("-----------------------------\n");
    for (int i = 0; i < 10000000; i++){
        printf("");
    }
    
}


// finished 0
// waiting  1
// ready    2
// running  3
CPUState* TaskManager::Schedule(CPUState* cpustate)
{
    taskTable();
    if (cpustate->eax == 6) {
        WaitTask(cpustate->ebx);
    }
    if(numTasks <= 0) {
        return cpustate;
    }
    if(currentTask >= 0) {
        tasks[currentTask].cpustate = cpustate;
    }    

    int findTask=(currentTask+1)%numTasks;
    while(tasks[findTask].taskState != 2) { // until the state is ready
        if(tasks[findTask].taskState == 1) { // if found task is waiting state
            int waitTaskIndex = 0;
            waitTaskIndex=getIndex(tasks[findTask].waitPid);
            if(waitTaskIndex > -1 && tasks[waitTaskIndex].taskState != 1) {
                if(tasks[waitTaskIndex].taskState == 0) {
                    printf("found new task\n");
                    tasks[findTask].waitPid = 0;
                    tasks[findTask].taskState = 2; // state is ready
                    continue;
                }
                else if (tasks[waitTaskIndex].taskState == 2) { // keep searching
                    printf("still searching\n");
                    findTask = waitTaskIndex;
                    continue;
                }
            }
        }
        findTask=(findTask+1)%numTasks;
    }
    currentTask = findTask;    
    return tasks[currentTask].cpustate;
}

    