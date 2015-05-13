#ifndef SERVICE_QUEUE_BROKER_H
#define SERVICE_QUEUE_BROKER_H

#include "zmq.hpp"
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

    vector<string> workers;
    int            currentWorkerIndex;

    mutex writeLock;
    mutex workersLock;

    condition_variable waitForWorkers;

    bool connected;
    bool interrupted;

    void connect();

    void registerWorker(const string &id);
    void removeWorker(const string &id);
    bool getNextWorker(string &workerName);

    void dispatchService();

    void send(const string &data);
    void sendMore(const string &data);
    void send(const string &data, bool more);

    void send(const zmq::message_t &msg);
    void sendMore(const zmq::message_t &msg);
    void send(const zmq::message_t &msg, bool more);

    string getAction(const string &data);
    string getMessageData(zmq::message_t &message);

public:
    broker();

    void run();
};

#endif //SERVICE_QUEUE_BROKER_H
