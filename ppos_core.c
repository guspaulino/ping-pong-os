#include "ppos.h"
#include <stdio.h>
#include <stdlib.h>

#define STACKSIZE 64*1024

task_t MainTask;
task_t *CurrentTask;
unsigned int id_counter = 1;

void ppos_init(){
    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    setvbuf (stdout, 0, _IONBF, 0) ;
    MainTask.id = 0;
    MainTask.status = EXECUTANDO;
    CurrentTask = &MainTask;
}

int task_init(task_t *task, void (*start_routine)(void *), void *arg){
    if(!task)
        return -1;

    getcontext(&task->context);
    
    char *stack;
    stack = malloc(STACKSIZE);
    if(stack){
        task->context.uc_stack.ss_sp = stack;
        task->context.uc_stack.ss_size = STACKSIZE;
        task->context.uc_stack.ss_flags = 0;
        task->context.uc_link = 0;
    } else 
        return -1;

    makecontext(&task->context, (void (*)(void)) start_routine, 1, arg);
    task->id = id_counter;
    task->status = PRONTA;
    id_counter++;
    #ifdef DEBUG
    printf("Task criada: %d - %s\n", task->id, (char *)arg);
    #endif
    return task->id;
}

int task_switch(task_t *task){
    if(!task)
        return -1;

    #ifdef DEBUG
    printf("Trocando de task %d -> %d\n", CurrentTask->id, task->id);
    #endif

    task_t *previous_task = CurrentTask;
    CurrentTask = task;

    return swapcontext(&previous_task->context, &task->context);
}

void task_exit(int exit_code){
    #ifdef DEBUG
    printf("Finalizando a task %d\n", CurrentTask->id);
    #endif
    task_switch(&MainTask);
}

int task_id(){
    return CurrentTask->id;
}