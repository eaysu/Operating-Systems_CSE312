
#include <syscalls.h>
 
using namespace myos;
using namespace myos::common;
using namespace myos::hardwarecommunication;
 
SyscallHandler::SyscallHandler(InterruptManager* interruptManager, uint8_t InterruptNumber)
:    InterruptHandler(interruptManager, InterruptNumber  + interruptManager->HardwareInterruptOffset())
{
}

SyscallHandler::~SyscallHandler()
{
}


void printf(char*);

int myos::getPid() {
    int ret;
    asm("int $0x80" : "=c"(ret) : "a"(1));
    return ret;
}

void myos:: fork() {
    asm("int $0x80" :: "a"(2));
}

void myos::exit() {
    asm("int $0x80" :: "a"(3));
}

int myos::getCPid() {
    int ret;
    asm("int $0x80" : "=c"(ret) : "a"(5));
    return ret;
}

int myos::waitpid(common::uint8_t wPid)
{
    int ret;
    asm("int $0x20" : "=c" (ret) : "a"(6), "b"(wPid));
    return ret;
}

uint32_t SyscallHandler::HandleInterrupt(uint32_t esp)
{
    CPUState* cpu = (CPUState*)esp;
    

    switch(cpu->eax)
    {
        case 1:
            cpu->ecx = InterruptHandler::os_getPid();
            break;
        // Syscall 2: fork
        case 2:
            cpu->ecx = InterruptHandler::os_fork(cpu);
            return InterruptHandler::HandleInterrupt(esp);
            break;
        // Syscall 3: exit
        case 3:
            if(InterruptHandler::os_exit()) {
                cpu->ecx = InterruptHandler::HandleInterrupt(esp);
            }
            break;   
        // Syscall 4: printf
        case 4:
            printf((char*)cpu->ebx);
            break;
        // Syscall 5: getPid
        case 5:
            cpu->ecx = InterruptHandler::os_getCPid();
            break;
        default:
            break;
    }
    return esp;
}

