#include "queue.h"
#include <stdio.h>

int main () {
    Queue queue = createQueue();
    queue.display(&queue);

    printf("push item a\n");
    queue.push(&queue, 'a');    
    printf("push item b\n");
    queue.push(&queue, 'b');
    printf("push item c\n");
    queue.push(&queue, 'c');

    queue.display(&queue);

    printf("peek item a: %c\n", queue.peek(&queue,0));
    printf("peek item c: %c\n", queue.peek(&queue,2));
    // printf("peek non-existent item d: %c\n", queue.peek(&queue,3)); Segfault

    /*printf("peek item %d\n", queue.peek(&queue));
    queue.display(&queue);

    printf("pop item %d\n", queue.pop(&queue));
    printf("pop item %d\n", queue.pop(&queue));
    queue.display(&queue);

    printf("pop item %d\n", queue.pop(&queue));
    queue.display(&queue);
    printf("push item 6\n");
    queue.push(&queue, 6);

    queue.display(&queue);*/
}
