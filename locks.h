#include <pthread.h>


class a4_rwlock {
 private:
    /* example of defining integers:
     *     int num_reader, num_writer;
     * example of defining conditional variables:
     *     pthread_cond_t contitional_variable;
     */
    /* Your Part II code goes here */
    int num_readers, num_writers, num_writers_waiting;
    pthread_mutex_t lock;
    pthread_cond_t thread_can_enter;
 public:
    a4_rwlock(){
        /* initializing the variables 
         * example: num_reader = num_writer = 0
         */
        /* Your Part II code goes here */
        num_readers = num_writers = num_writers_waiting = 0;
        pthread_mutex_init(&lock, NULL);
        pthread_cond_init(&thread_can_enter, NULL);
    }
    
    void reader_enter() {
        /* Your Part II code goes here */
        pthread_mutex_lock(&lock);
        // while(num_writers_waiting > 0) {
        //     pthread_cond_wait(&thread_can_enter, &lock);
        // }
        while(num_writers > 0) {
            pthread_cond_wait(&thread_can_enter, &lock);
        }
        num_readers++;
        pthread_mutex_unlock(&lock);
    }

    void reader_exit() {
        /* Your Part II code goes here */
        pthread_mutex_lock(&lock);
        num_readers--;
        pthread_cond_broadcast(&thread_can_enter);
        pthread_mutex_unlock(&lock);
    }
    
    void writer_enter() {
        /* Your Part II code goes here */
        pthread_mutex_lock(&lock);
        // num_writers_waiting++;
        while (num_readers > 0 || num_writers > 0){
            pthread_cond_wait(&thread_can_enter, &lock);
        }
        num_writers++;
        pthread_mutex_unlock(&lock);
    }
    
    void writer_exit() {
        /* Your Part II code goes here */
        pthread_mutex_lock(&lock);
        num_writers--;
        // num_writers_waiting--;
        pthread_cond_broadcast(&thread_can_enter);
        pthread_mutex_unlock(&lock);
    }
};
