#include "ppos.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <valgrind/valgrind.h>
#include <signal.h>
#include <sys/time.h>
#define STACKSIZE 64*1024

task_t MainTask; 
task_t DispatcherTask;
task_t *CurrentTask;
unsigned int id_counter = 0;
queue_t *ready_tasks = NULL; 
struct sigaction action; // ação para o sinal SIGALARM (preempção de tarefa)
struct itimerval timer; // timer para preempção de tarefa
unsigned int ticks = 0;
unsigned int switch_tick = 0; // tick em que ocorreu a troca para a tarefa atual para calcular tempo de processador

// Função para printar a fila de prontas
void print_elem(void *elem){
    task_t *task = (task_t *) elem;
    printf("Id: %d - DPrio: %d\n", task->id, task->dynamic_prio);
}

unsigned int systime(){
    return ticks;
}

void print_exit(){
    unsigned int exec_time = systime() - CurrentTask->task_start;
    printf("Task %d exit: execution time %d ms, processor time %d ms, %u activations\n", CurrentTask->id, exec_time, CurrentTask->processor_time, CurrentTask->activations);
}

void signal_handler(){
    if(!CurrentTask->isSystemTask){
        CurrentTask->quantum_counter--;

        if(CurrentTask->quantum_counter <= 0)
            task_yield();
    }
        
    ticks++;
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
    task_t *curr = (task_t *) ready_tasks->next; // Variável para varrer a lista toda comparando a prioridade
    while (curr != (task_t *) ready_tasks){
        if(curr->dynamic_prio < next_task->dynamic_prio){
            next_task->dynamic_prio--;
            next_task = curr;
        } else 
            curr->dynamic_prio--;
            
        curr = curr->next;
    }

    next_task->dynamic_prio = next_task->static_prio; // Depois de escolher a tarefa, seta a prioridade dela de volta para a estática
    return (task_t *) next_task;
}

void dispatcher(){
    queue_remove(&ready_tasks, (queue_t *) &DispatcherTask);

    
    while(queue_size(ready_tasks)){
        
        task_t *next_task = scheduler();
        if(next_task){
            queue_remove(&ready_tasks, (queue_t *) next_task);
            next_task->quantum_counter = 20;

            next_task->activations++;

            task_switch(next_task);
            switch (next_task->status)
            {
            case PRONTA:
                break;
            case TERMINADA: // Libera a memória alocada para a tarefa
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
    action.sa_handler = signal_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(SIGALRM, &action, 0) < 0){
        perror("Erro em sigaction: ");
        exit(1);
    }
    
    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    setvbuf (stdout, 0, _IONBF, 0) ;
    
    MainTask.status = EXECUTANDO;
    MainTask.activations = 1;
    task_init(&MainTask, NULL, "MainTask");
    CurrentTask = &MainTask;


    DispatcherTask.isSystemTask = 1;
    timer.it_value.tv_usec = 1000; // primeiro disparo em 1 milisegundo
    timer.it_value.tv_sec = 0;
    timer.it_interval.tv_usec = 1000; // disparos seguintes a cada 1 milisegundo
    timer.it_interval.tv_sec = 0; 

    if(setitimer(ITIMER_REAL, &timer, 0) < 0){
        perror ("Erro em setitimer: ") ;
        exit (1) ;
    }

    task_init(&DispatcherTask, *dispatcher, "Dispatcher"); // Passa o controle para o dispatcher depois de iniciar o SO
}

int task_init(task_t *task, void (*start_routine)(void *), void *arg){
    if(!task)
        return -1;

    getcontext(&task->context);
    
    char *stack;
    stack = malloc(STACKSIZE);
    if(stack){  // aloca stack para a tarefa
        task->context.uc_stack.ss_sp = stack;
        task->context.uc_stack.ss_size = STACKSIZE;
        task->context.uc_stack.ss_flags = 0;
        task->context.uc_link = 0;

        task->vg_id = VALGRIND_STACK_REGISTER(stack, stack + STACKSIZE); // necessário para valgrind encontrar essa stack
    } else 
        return -1;

    
    task->id = id_counter;
    task->waiting_tasks = NULL;

    if (task != &MainTask){
        makecontext(&task->context, (void (*)(void)) start_routine, 1, arg);    
        task->status = PRONTA;
        task->activations = 0;
        
        if(!queue_append(&ready_tasks, (queue_t *) task) == 0)
            return -1;
    }
    task_setprio(task, 0); // prioridade padrão é 0

    if(task != &DispatcherTask)
        task->isSystemTask = 0;
    

    task->task_start = systime();
    task->processor_time = 0;
    

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
    previous_task->processor_time += systime() - switch_tick;
    switch_tick = systime();
    return swapcontext(&previous_task->context, &task->context);
}

void task_exit(int exit_code){
    #ifdef DEBUG
    printf("Finalizando a task %d\n", CurrentTask->id);
    #endif

    print_exit();
    if(CurrentTask != &DispatcherTask){
        CurrentTask->exit_code = exit_code;
        task_t *head = CurrentTask->waiting_tasks;
        
        if(head){
            task_t *task = head;
            do {
                task_t *next_task = task->next;
                task_awake(task, &CurrentTask->waiting_tasks);
                task = next_task;
            } while (task != head);
        }
        
        CurrentTask->status = TERMINADA;
        DispatcherTask.activations++;
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
    DispatcherTask.activations++;
    task_switch(&DispatcherTask);
}

void task_suspend(task_t **queue){
    CurrentTask->status = SUSPENSA;
    queue_append((queue_t **) queue, (queue_t *) CurrentTask);
    task_switch(&DispatcherTask);
}

void task_awake(task_t *task, task_t **queue){
    queue_remove((queue_t **) queue, (queue_t *) task);
    task->status = PRONTA;
    queue_append(&ready_tasks, (queue_t *) task);
}

int task_wait(task_t *task){
    if(task == NULL || task->status == TERMINADA)
        return -1;

    task_suspend(&task->waiting_tasks);
    return task->exit_code;
    // task_suspend -> salvar contexto atual da tarefa em execução
    // quando a *task terminar, salvar numa variável dela o exit_code
    // quando a "tarefa atual" acorddar, restaurar o contexto pra cá e retornar o exit da *task 
    
}
