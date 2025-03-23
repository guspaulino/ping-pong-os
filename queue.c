#include "queue.h"
#include <stdio.h>

int queue_size (queue_t *queue){
    if(!queue)
        return 0;

    int count = 1;
    queue_t *first = queue;
    while(queue->next != first){
        count += 1;
        queue = queue->next;
    }
    
    return count;
}

void queue_print (char *name, queue_t *queue, void print_elem (void*) ){
    return;
}

int queue_append (queue_t **queue, queue_t *elem){
    if(!queue)
        return -1; // Queue doesn't exist

    if(!elem)
        return -2; // Elem doesn't exist

    if(elem->next || elem->prev)
        return -3; // Elem is already in another queue

    if(!(*queue)){
        *queue = elem;
        elem->next = elem;
        elem->prev = elem;
    } else {
        queue_t *aux = *queue;
        // *queue must be the first element of the list
        while (aux->next != *queue)
            aux = aux->next;
        
        aux->next = elem;
        elem->prev = aux;
        elem->next = *queue;
        (*queue)->prev = elem;
    }
    
    return 0;
}

int queue_remove (queue_t **queue, queue_t *elem){
    if(!queue) // queue doesn't exist
        return -1;

    if(!(*queue)) // queue is empty 
        return -2;

    if(!elem) // elem doesn't exist
        return -3;

    if((*queue)->next == *queue){ // queue has just one element
        if(*queue == elem){
            elem->prev = NULL; 
            elem->next = NULL;
            *queue = NULL;
            return 0;
        } else {
            return -4; // elem is not in the list
        }
        
    }

    queue_t *aux = *queue;
    while (aux->next != *queue && aux != elem) // search for the elem inside of the list 
        aux = aux->next;

    if(aux != elem)
        return -4; // elem is not in the list

    queue_t *prev = aux->prev; // previous node from node to be excluded
    queue_t *next = aux->next; // next node from node to be exluded

    prev->next = aux->next;
    next->prev = aux->prev;

    if(*queue == aux)
        *queue = aux->next;
    
    aux->next = NULL;
    aux->prev = NULL;

    return 0;
}
