#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "threadpool.h"

void freeThreadpool(threadpool *pool)
{
    free(pool->threads);
    free(pool);
}

threadpool* create_threadpool(int num_threads_in_pool)
{
    if(num_threads_in_pool > MAXT_IN_POOL || num_threads_in_pool < 1)
    {
        perror("error: <sys_call>\n");
        return NULL;
    }
    threadpool *myThreadPool = (threadpool*)malloc(sizeof(threadpool));
    if(myThreadPool == NULL)
    {
        perror("error: <sys_call>\n");
        return NULL;
    }
    myThreadPool->num_threads = num_threads_in_pool;
    myThreadPool->threads = (pthread_t *)malloc(myThreadPool->num_threads*sizeof(pthread_t));
    if(myThreadPool->threads == NULL)
    {
        perror("error: <sys_call>\n");
        return NULL;
    }
    myThreadPool->qhead = NULL;
    myThreadPool->qtail = NULL;
    myThreadPool->qsize = 0;
    int mutex_init = pthread_mutex_init(&(myThreadPool->qlock),NULL);
    if(mutex_init)
    {
        perror("error: <sys_call>\n");
        freeThreadpool(myThreadPool);
        return NULL;
    }
    int cond = pthread_cond_init(&(myThreadPool->q_empty),NULL);
    if(cond)
    {
        perror("error: <sys_call>\n");
        pthread_mutex_destroy(&(myThreadPool->qlock));
        freeThreadpool(myThreadPool);
        return NULL;
    }
    int cond2 = pthread_cond_init(&(myThreadPool->q_not_empty),NULL);
    if(cond)
    {
        perror("error: <sys_call>\n");
        pthread_mutex_destroy(&(myThreadPool->qlock));
        pthread_cond_destroy(&(myThreadPool->q_empty));
        freeThreadpool(myThreadPool);
        return NULL;
    }
    myThreadPool->shutdown = 0;
    myThreadPool->dont_accept = 0;
    for(int i = 0;i<myThreadPool->num_threads;i++)
    {
        if(pthread_create(&(myThreadPool->threads[i]),NULL,do_work,myThreadPool))
        {
            perror("error: <sys_call>\n");
            pthread_mutex_destroy(&(myThreadPool->qlock));
            pthread_cond_destroy(&(myThreadPool->q_empty));
            pthread_cond_destroy(&(myThreadPool->q_not_empty));
            freeThreadpool(myThreadPool);
            return NULL;
        }
    }
    return myThreadPool;
}

void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)
{
     if(dispatch_to_here == NULL)
    {
        perror("error: <sys_call>\n");
        return;
    }
    pthread_mutex_lock(&(from_me->qlock));
    if(from_me->dont_accept == 1)
        return;
    pthread_mutex_unlock(&(from_me->qlock));
    work_t *work = (work_t*)malloc(sizeof(work_t));
    if(work == NULL)
    {
        perror("error: <sys_call>\n");
        return;
    }
    work->routine = dispatch_to_here;
    work->arg = arg;
    work->next = NULL;

    pthread_mutex_lock(&(from_me->qlock));
    if(from_me->dont_accept == 1)
    {
        free(work);
        return;
    }

    if(from_me->qsize == 0)
    {
        from_me->qhead = work;
        from_me->qtail = work;
    }
    else
    {
        from_me->qtail->next = work;
        from_me->qtail = from_me->qtail->next;
    }
    from_me->qsize++;
    pthread_cond_signal(&(from_me->q_not_empty));
    pthread_mutex_unlock(&(from_me->qlock));
}

void* do_work(void* p)
{
    threadpool * tpool = (threadpool*) p;    
    while (1) 
    {
        pthread_mutex_lock(&(tpool->qlock));
        
        if (tpool->qsize == 0) 
        {
            if (tpool->shutdown == 1) {
                pthread_mutex_unlock(&(tpool->qlock));
                pthread_exit(NULL);
            }
                    
            if (tpool->qhead == NULL) {
                pthread_cond_wait(&tpool->q_not_empty , &tpool->qlock);
            }
            
            if (tpool->shutdown == 1) {
                pthread_mutex_unlock(&(tpool->qlock));
                pthread_exit(NULL);
            }
        }
        
        
        work_t * work = tpool->qhead;
        
            

        if(tpool->qsize == 0) 
        {
            tpool->qhead = NULL;
            tpool->qtail = NULL;
        }
        else {
            tpool->qhead = work->next;
        }
                
        tpool->qsize--;

        if(tpool->qsize == 0 && tpool->dont_accept) 
        {
        //now signal that the queue is empty.
            pthread_cond_signal(&(tpool->q_empty));
        }
        
        pthread_mutex_unlock(&(tpool->qlock));
        (work->routine) (work->arg);
        free(work);
        
        }
    
    return NULL;
}


void destroy_threadpool(threadpool* destroyme){
    if(destroyme == NULL)
        return;
    destroyme->dont_accept = 1;
    while(destroyme->qsize > 0)
    {
        pthread_cond_wait(&(destroyme->q_empty),&(destroyme->qlock));
    }
    destroyme->shutdown = 1;
    pthread_cond_broadcast(&(destroyme->q_not_empty));
    pthread_mutex_unlock(&(destroyme->qlock));
    void* satus;
    for(int i=0;i<destroyme->num_threads;i++)
    {
        pthread_join(destroyme->threads[i],&satus);
    }
    pthread_mutex_destroy(&(destroyme->qlock));
    pthread_cond_destroy(&(destroyme->q_empty));
    pthread_cond_destroy(&(destroyme->q_not_empty));
   freeThreadpool(destroyme);
}