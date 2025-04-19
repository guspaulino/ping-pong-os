#include "ppos.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <valgrind/valgrind.h>
#define STACKSIZE 64*1024

task_t MainTask;
task_t DispatcherTask;
task_t *CurrentTask;
unsigned int id_counter = 1;
queue_t *ready_tasks = NULL; 
unsigned int user_tasks = 0;

void print_elem(void *elem){
    task_t *task = (task_t *) elem;
    printf("Id: %d - DPrio: %d\n", task->id, task->dynamic_prio);
}

void task_setprio(task_t *task, int prio){
    if(!task){
        CurrentTask->static_prio = prio;
        CurrentTask->dynamic_prio = prio;
    }
        
    task->static_prio = prio;
    task->dynamic_prio = prio;
}

int task_getprio(task_t *task){
    if(!task)
        return CurrentTask->static_prio;

    return task->static_prio;
}

task_t *scheduler(){
    #ifdef DEBUG
    queue_print("Tasks ready: ", (queue_t *) ready_tasks, *print_elem);
    #endif
    task_t *next_task = (task_t *) ready_tasks;
    task_t *curr = (task_t *) ready_tasks->next;
    while (curr != (task_t *) ready_tasks){
        if(curr->dynamic_prio < next_task->dynamic_prio)
            next_task = curr;

        curr = curr->next;
    }
    
    if(next_task != curr)
        curr->dynamic_prio -= 1;

    curr = curr->next;
    while (curr != (task_t *) ready_tasks){
        if(curr != next_task)
            curr->dynamic_prio -= 1;
        else
            curr->dynamic_prio = curr->static_prio; // reseta a prio da task escolhida para a prio estática

        curr = curr->next;            
    }

    
    return (task_t *) next_task;
}

void dispatcher(){
    queue_remove(&ready_tasks, (queue_t *) &DispatcherTask);
    user_tasks--;

    while(user_tasks){
        task_t *next_task = scheduler();
        if(next_task){
            queue_remove(&ready_tasks, (queue_t *) next_task);
            user_tasks--;

            task_switch(next_task);

            switch (next_task->status)
            {
            case PRONTA:
                break;
            case TERMINADA:
                free(next_task->context.uc_stack.ss_sp);
                VALGRIND_STACK_DEREGISTER(next_task->vg_id);
                break;
            case SUSPENSA:
                break;
            case EXECUTANDO:
                break;

            default:
                break;
            }
        }
    }

    task_exit(0);
}

void ppos_init(){
    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    setvbuf (stdout, 0, _IONBF, 0) ;
    MainTask.id = 0;
    MainTask.status = EXECUTANDO;
    CurrentTask = &MainTask;

    task_init(&DispatcherTask, *dispatcher, "Dispatcher");
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

        task->vg_id = VALGRIND_STACK_REGISTER(stack, stack + STACKSIZE);
    } else 
        return -1;

    makecontext(&task->context, (void (*)(void)) start_routine, 1, arg);
    task->id = id_counter;
    task->status = PRONTA;
    task_setprio(task, 0);
    if(queue_append(&ready_tasks, (queue_t *) task) == 0)
        user_tasks++;
    else // erro ao inserir na fila de tasks
        return -1;

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

    task->status = EXECUTANDO;
    return swapcontext(&previous_task->context, &task->context);
}

void task_exit(int exit_code){
    #ifdef DEBUG
    printf("Finalizando a task %d\n", CurrentTask->id);
    #endif
    if(CurrentTask != &DispatcherTask){
        CurrentTask->status = TERMINADA;
        task_switch(&DispatcherTask);
    }
    else{
        free(DispatcherTask.context.uc_stack.ss_sp);
        exit(0);
    }
        
}

int task_id(){
    return CurrentTask->id;
}

void task_yield(){
    #ifdef DEBUG
    printf("Yielding a task %d\n", CurrentTask->id);
    #endif
    CurrentTask->status = PRONTA;
    queue_append(&ready_tasks, (queue_t *) CurrentTask);
    user_tasks++;
    task_switch(&DispatcherTask);
}
