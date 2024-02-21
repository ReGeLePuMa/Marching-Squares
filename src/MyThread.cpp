#include "MyThread.hpp"

// Constructorul clasei
MyThread::MyThread(long id, void *(*func)(void *), void *arg)
{
    this->id = id;
    this->func = func;
    this->arg = arg;
}

// Getteri si setteri
long MyThread::getID()
{
    return this->id;
}

void MyThread::setID(long id)
{
    this->id = id;
}

void MyThread::setArg(void *arg)
{
    this->arg = arg;
}

void MyThread::setFunc(void *(*func)(void *))
{
    this->func = func;
}

// Functie ce porneste pthread-ul cu functia si argumentul setate
void MyThread::start()
{
    if (pthread_create(&this->thread, NULL, this->func, this->arg))
    {
        fprintf(stderr, "Eroare la crearea thread-ului\n");
        exit(1);
    }
}

// Destructorul clasei in care asteapta thread-ul sa se termine
MyThread::~MyThread()
{
    if (pthread_join(this->thread, NULL))
    {
        fprintf(stderr, "Eroare la asteptarea thread-ului\n");
        exit(1);
    }
}
