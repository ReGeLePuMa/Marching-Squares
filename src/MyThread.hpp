#ifndef MY_THREAD_HPP
#define MY_THREAD_HPP

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

// Clasa creata pentru o interfata mai usoara cu pthread-urile
class MyThread
{
    // Contine un pthread, id-ul sau si tot ce ii trebuie pentru a rula o functie
private:
    pthread_t thread;
    long id;
    void *arg;
    void *(*func)(void *);

public:
    MyThread(long id, void *(*func)(void *), void *arg);
    ~MyThread();

    long getID();
    void setID(long id);
    void setArg(void *arg);
    void setFunc(void *(*func)(void *));
    void start();
};
#endif