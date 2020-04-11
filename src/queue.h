#ifndef QUEUE_HEADER
#define QUEUE_HEADER
/* 
 * queue[0] = prev
 * queue[1] = next
 */
typedef void *queue[2];

#define QUEUE_PREV(q) (*(queue **) &((*(q))[0]))
#define QUEUE_NEXT(q) (*(queue **) &((*(q))[1]))
#define QUEUE_PREV_NEXT(q) QUEUE_PREV(QUEUE_NEXT(q))
#define QUEUE_NEXT_PREV(q) QUEUE_NEXT(QUEUE_PREV(q))

#define QUEUE_INIT(q)                       \
    do{                                     \
        QUEUE_PREV(q) = (q);                \
        QUEUE_NEXT(q) = (q);                \
    }while(0)

#define QUEUE_EMPTY(q)                      \
    ((queue *) (q) == QUEUE_NEXT(q) && (queue *) (q) == QUEUE_PREV(q))
   
#define QUEUE_ADD(q, n)                     \
    do{                                     \
        QUEUE_PREV(QUEUE_NEXT(q)) = (n);    \
        QUEUE_NEXT(n) = QUEUE_NEXT(q);      \
        QUEUE_NEXT(q) = (n);                \
        QUEUE_PREV(n) = (q);                \
    }while(0)

#define QUEUE_ADD_HEAD(q, n)                \
    do{                                     \
        QUEUE_NEXT(n) = QUEUE_NEXT(q);      \
        QUEUE_PREV_NEXT(q) = (n);           \
        QUEUE_PREV(n) = (q);                \
        QUEUE_NEXT(q) = (n);                \
    }while(0)

#define QUEUE_ADD_HEAD_GET(q, n, h)         \
    do{                                     \
        QUEUE_ADD_HEAD(q, n);               \
        (h) = (n);                          \
    }while(0)

#define QUEUE_ADD_TAIL(q, n)                \
    do{                                     \
        QUEUE_PREV(n) = QUEUE_PREV(q);      \
        QUEUE_NEXT_PREV(q) = (n);           \
        QUEUE_NEXT(n) = (q);                \
        QUEUE_PREV(q) = (n);                \
    }while(0)

#define QUEUE_ADD_TAIL_GET(q, n, t)         \
    do{                                     \
        QUEUE_ADD_TAIL(q, n);               \
        (t) = (n);                          \
    }while(0)

#define QUEUE_REMOVE(q)                     \
    do{                                     \
        QUEUE_NEXT_PREV(q) = QUEUE_NEXT(q); \
        QUEUE_PREV_NEXT(q) = QUEUE_PREV(q); \
    }while(0)

#define QUEUE_MOVE(q, n)                    \
    do{                                     \
        if(QUEUE_EMPTY(q)){                 \
            QUEUE_INIT(n);                  \
        }else{                              \
            QUEUE_NEXT(n) = QUEUE_NEXT(q);  \
            QUEUE_PREV(n) = QUEUE_PREV(q);  \
            QUEUE_NEXT_PREV(n) = (n);       \
            QUEUE_PREV_NEXT(n) = (n);       \
            QUEUE_INIT(q);                  \
        }                                   \
    }while(0)

#endif