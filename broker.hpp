//
// Created by Denis Savenko on 31.03.15.
//

#ifndef SERVICE_QUEUE_BROKER_H
#define SERVICE_QUEUE_BROKER_H


#include "zmq.hpp"
#include <map>
#include <list>
#include <vector>
#include <mutex>
#include <condition_variable>

using namespace std;

class broker
{

private:
    zmq::context_t *ctx;

    zmq::socket_t *input;
    zmq::socket_t *output;
    zmq::socket_t *service;

    mutex writeLock;

    vector<string> workers;
    int            currentWorkerIndex;

    mutex workersLock;
    condition_variable waitForWorkers;

    bool connected;
    bool interrupted;


    void connect();

    void registerWorker(const string &id);

    void removeWorker(const string &id);

    bool getNextWorker(string &workerName);

    string getAction(const string &data);

    void send(const string &data);

    void sendMore(const string &data);

    void send(const string &data, bool more);

    string getMessageData(zmq::message_t &message);

    void dispatchService();

public:
    broker();

    void run();
};

#endif //SERVICE_QUEUE_BROKER_H
