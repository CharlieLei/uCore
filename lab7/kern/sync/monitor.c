#include <stdio.h>
#include <monitor.h>
#include <kmalloc.h>
#include <assert.h>


// Initialize monitor.
void     
monitor_init (monitor_t * mtp, size_t num_cv) {
    int i;
    assert(num_cv>0);
    mtp->next_count = 0;
    mtp->cv = NULL;
    sem_init(&(mtp->mutex), 1); //unlocked
    sem_init(&(mtp->next), 0);
    mtp->cv =(condvar_t *) kmalloc(sizeof(condvar_t)*num_cv);
    assert(mtp->cv!=NULL);
    for(i=0; i<num_cv; i++){
        mtp->cv[i].count=0;
        sem_init(&(mtp->cv[i].sem),0);
        mtp->cv[i].owner=mtp;
    }
}

// Unlock one of threads waiting on the condition variable. 
void 
cond_signal (condvar_t *cvp) {
   //LAB7 EXERCISE1: YOUR CODE
   cprintf("cond_signal begin: cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
   monitor_t *monitor = cvp->owner;
   if (cvp->count > 0) {
       // 如果大于0，这表示当前有执行cond_wait而睡眠的进程A，
       // 因此需要唤醒等待在cv.sem上睡眠的进程A
       monitor->next_count ++;
       up(&(cvp->sem));
       // 由于只允许一个进程在管程中执行，所以一旦进程B唤醒了别人（进程A），那么自己就需要睡眠
       // signal的阻塞是想让等待的进程快快执行
       down(&(monitor->next));
       monitor->next_count --;
   }
   // 如果不大于0，则表示当前没有执行cond_wait而睡眠的进程，因此就没有被唤醒的对象了
   cprintf("cond_signal end: cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
}

// Suspend calling thread on a condition variable waiting for condition Atomically unlocks 
// mutex and suspends calling thread on conditional variable after waking up locks mutex. Notice: mp is mutex semaphore for monitor's procedures
void
cond_wait (condvar_t *cvp) {
    //LAB7 EXERCISE1: YOUR CODE
    cprintf("cond_wait begin:  cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
    monitor_t *monitor = cvp->owner;
    cvp->count ++;
    if (monitor->next_count > 0) {
        // 如果monitor.next_count如果大于0，表示有大于等于1个进程
        // 执行cond_signal函数且睡着了，就睡在了monitor.next信号量上
        up(&(monitor->next));
    } else {
        // 如果monitor.next_count如果小于等于0，表示目前没有进程执行cond_signal
        // 函数且睡着了，那需要唤醒的是由于互斥条件限制而无法进入管程的进程
        up(&(monitor->mutex));
    }
    // 只允许一个进程在管程中执行，所以自己要睡眠
    // wait的阻塞是因为缺少资源而不得不阻塞
    down(&(cvp->sem));
    cvp->count --;
    cprintf("cond_wait end:  cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
}
