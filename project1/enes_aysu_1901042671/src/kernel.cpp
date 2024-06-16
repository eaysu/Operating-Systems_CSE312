
#include <common/types.h>
#include <gdt.h>
#include <memorymanagement.h>
#include <hardwarecommunication/interrupts.h>
#include <syscalls.h>
#include <hardwarecommunication/pci.h>
#include <drivers/driver.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/vga.h>
#include <drivers/ata.h>
#include <gui/desktop.h>
#include <gui/window.h>
#include <multitasking.h>

#include <drivers/amd_am79c973.h>
#include <net/etherframe.h>
#include <net/arp.h>
#include <net/ipv4.h>
#include <net/icmp.h>
#include <net/udp.h>
#include <net/tcp.h>

// #define GRAPHICSMODE

using namespace myos;
using namespace myos::common;
using namespace myos::drivers;
using namespace myos::hardwarecommunication;
using namespace myos::gui;
using namespace myos::net;

// For the printf function
void printf(char* str)
{
    static uint16_t* VideoMemory = (uint16_t*)0xb8000;

    static uint8_t x=0,y=0;

    for(int i = 0; str[i] != '\0'; ++i)
    {
        switch(str[i])
        {
            case '\n':
                x = 0;
                y++;
                break;
            default:
                VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0xFF00) | str[i];
                x++;
                break;
        }

        if(x >= 80)
        {
            x = 0;
            y++;
        }

        if(y >= 25)
        {
            for(y = 0; y < 25; y++)
                for(x = 0; x < 80; x++)
                    VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0xFF00) | ' ';
            x = 0;
            y = 0;
        }
    }
}

void printfHex(uint8_t key)
{
    char *foo = "00";
    char *hex = "0123456789ABCDEF";
    foo[0] = hex[(key >> 4) & 0xF];
    foo[1] = hex[key & 0xF];
    printf(foo);
}
void printfHex16(uint16_t key)
{
    printfHex((key >> 8) & 0xFF);
    printfHex(key & 0xFF);
}
void printfHex32(uint32_t key)
{
    printfHex((key >> 24) & 0xFF);
    printfHex((key >> 16) & 0xFF);
    printfHex((key >> 8) & 0xFF);
    printfHex(key & 0xFF);
}

class PrintfKeyboardEventHandler : public KeyboardEventHandler
{
public:
    void OnKeyDown(char c)
    {
        char *foo = " ";
        foo[0] = c;
        printf(foo);
    }
};

class MouseToConsole : public MouseEventHandler
{
    int8_t x, y;

public:
    MouseToConsole()
    {
        uint16_t *VideoMemory = (uint16_t *)0xb8000;
        x = 40;
        y = 12;
        VideoMemory[80 * y + x] = (VideoMemory[80 * y + x] & 0x0F00) << 4 | (VideoMemory[80 * y + x] & 0xF000) >> 4 | (VideoMemory[80 * y + x] & 0x00FF);
    }

    virtual void OnMouseMove(int xoffset, int yoffset)
    {
        static uint16_t *VideoMemory = (uint16_t *)0xb8000;
        VideoMemory[80 * y + x] = (VideoMemory[80 * y + x] & 0x0F00) << 4 | (VideoMemory[80 * y + x] & 0xF000) >> 4 | (VideoMemory[80 * y + x] & 0x00FF);

        x += xoffset;
        if (x >= 80)
            x = 79;
        if (x < 0)
            x = 0;
        y += yoffset;
        if (y >= 25)
            y = 24;
        if (y < 0)
            y = 0;

        VideoMemory[80 * y + x] = (VideoMemory[80 * y + x] & 0x0F00) << 4 | (VideoMemory[80 * y + x] & 0xF000) >> 4 | (VideoMemory[80 * y + x] & 0x00FF);
    }
};

class PrintfUDPHandler : public UserDatagramProtocolHandler
{
public:
    void HandleUserDatagramProtocolMessage(UserDatagramProtocolSocket *socket, common::uint8_t *data, common::uint16_t size)
    {
        char *foo = " ";
        for (int i = 0; i < size; i++)
        {
            foo[0] = data[i];
            printf(foo);
        }
    }
};

class PrintfTCPHandler : public TransmissionControlProtocolHandler
{
public:
    bool HandleTransmissionControlProtocolMessage(TransmissionControlProtocolSocket *socket, common::uint8_t *data, common::uint16_t size)
    {
        char *foo = " ";
        for (int i = 0; i < size; i++)
        {
            foo[0] = data[i];
            printf(foo);
        }

        if (size > 9 && data[0] == 'G' && data[1] == 'E' && data[2] == 'T' && data[3] == ' ' && data[4] == '/' && data[5] == ' ' && data[6] == 'H' && data[7] == 'T' && data[8] == 'T' && data[9] == 'P')
        {
            socket->Send((uint8_t *)"HTTP/1.1 200 OK\r\nServer: MyOS\r\nContent-Type: text/html\r\n\r\n<html><head><title>My Operating System</title></head><body><b>My Operating System</b> http://www.AlgorithMan.de</body></html>\r\n", 184);
            socket->Disconnect();
        }

        return true;
    }
};

void sysprintf(char *str)
{
    asm("int $0x80" : : "a"(4), "b"(str));
}

void collatz(int n) {
    printf("\n");
    printf("Output ");
    printfHex32(n);
    printf(":");
    while (n != 1) {
        printf(" ");
        printfHex16(n);
        if (n % 2 == 0)
            n /= 2;
        else
            n = 3 * n + 1;
    }
    printf("\n");
}

int long_running_program(int n) {
    int result = 0;
    for(int i = 0;i < n;i++) {
        for(int j = 0;j < n;j++) {
            result += i * j;
        }
    }
    return result;
}

void collatzFunction(){
    printf("### Collatz Started ###\n");
    for (int i = 1; i < 100; i++) {
        collatz(i);
    }
    printf("### Collatz Finished ###\n");
    // interval
    for (int a = 0;a < 1000000000;a++) { 
        printf("");
    }
}

void longRunningProgramFunction(int duration) {
    printf("### Long Running Program Started ###\n");
    int result = long_running_program(duration);
    printf("Result: ");
    printfHex32(result);
    printf("\n");
    printf("### Long Running Program Finished ###\n");
    // interval
    for (int a = 0;a < 1000000000;a++) { 
        printf("");
    }
    
}

void taskCollatzAndLongProgram() {
    fork();
    int pid = getCPid();
    if(pid == 0) {
        collatzFunction();
        exit();
    } /*else {
        waitpid(pid);
    }*/
    fork();
    int pid2 = getCPid();
    if(pid2 == 0) {
        collatzFunction();
        exit();
    } /*else {
        waitpid(0);
    }*/
    fork();
    int pid3 = getCPid();
    if(pid3 == 0) {
        collatzFunction();
        exit();
    } /*else {
        waitpid(0);
    }*/
    fork();
    int pid4 = getCPid();
    if(pid4 == 0) {
        longRunningProgramFunction(250);
        exit();
    } /*else {
        waitpid(0);
    }*/
    fork();
    int pid5 = getCPid();
    if(pid5 == 0) {
        longRunningProgramFunction(500);
        exit();
    } /*else {
        waitpid(0);
    }*/
    fork();
    int pid6 = getCPid();
    if(pid6 == 0) {
        longRunningProgramFunction(1000);
        exit();
    } else {
        waitpid(0);
    }
    exit();
    while(1);
}

void tasktask() {
    fork();
    int pid = getCPid();
    if(pid == 0) {
        printf("child\n");
        exit();
    } else {
        printf("parent1\n");
        waitpid(pid);
        printf("parent2\n");
    }
    exit();
    while (1);
}

typedef void (*constructor)();
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;
extern "C" void callConstructors()
{
    for (constructor *i = &start_ctors; i != &end_ctors; i++)
        (*i)();
}

extern "C" void kernelMain(const void *multiboot_structure, uint32_t /*multiboot_magic*/)
{
    printf("Hello World! --- http://www.AlgorithMan.de\n");

    GlobalDescriptorTable gdt;

    uint32_t *memupper = (uint32_t *)(((size_t)multiboot_structure) + 8);
    size_t heap = 10 * 1024 * 1024;
    MemoryManager memoryManager(heap, (*memupper) * 1024 - heap - 10 * 1024);

    /*printf("heap: 0x");
    printfHex((heap >> 24) & 0xFF);
    printfHex((heap >> 16) & 0xFF);
    printfHex((heap >> 8) & 0xFF);
    printfHex((heap) & 0xFF);*/

    void *allocated = memoryManager.malloc(1024);
    /*printf("\nallocated: 0x");
    printfHex(((size_t)allocated >> 24) & 0xFF);
    printfHex(((size_t)allocated >> 16) & 0xFF);
    printfHex(((size_t)allocated >> 8) & 0xFF);
    printfHex(((size_t)allocated) & 0xFF);
    printf("\n");*/

    TaskManager taskManager;
    //Task task1(&gdt, taskA);
    //Task task2(&gdt, taskB);
    //taskManager.AddTask(&task1);
    //taskManager.AddTask(&task2);
    Task taskMain(&gdt, taskCollatzAndLongProgram);
    taskManager.AddTask(&taskMain);

    InterruptManager interrupts(0x20, &gdt, &taskManager);
    SyscallHandler syscalls(&interrupts, 0x80);

    DriverManager drvManager;

    PrintfKeyboardEventHandler kbhandler;
    KeyboardDriver keyboard(&interrupts, &kbhandler);

    drvManager.AddDriver(&keyboard);

    MouseToConsole mousehandler;
    MouseDriver mouse(&interrupts, &mousehandler);

    drvManager.AddDriver(&mouse);

    PeripheralComponentInterconnectController PCIController;
    PCIController.SelectDrivers(&drvManager, &interrupts);

    drvManager.ActivateAll();

    interrupts.Activate();

    while (1)
    {
    }
}
