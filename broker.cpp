#include "broker.hpp"
#include "main.hpp"
#include <iostream>

#include <json/json.h>
#include <thread>

using namespace std;

void broker::run()
{
    connect();

    thread serviceThread = thread(&broker::dispatchService, this);

    zmq::pollitem_t pollItems[] = {{*input,  0, ZMQ_POLLIN, 0}};

    while (true)
    {
        zmq::poll(pollItems, 1, 1000);

        if (pollItems[0].revents & ZMQ_POLLIN)
        {
            string data, action;
            zmq::message_t message;

            input->recv(&message);

            data   = getMessageData(message);
            action = getAction(data);

            LOG << "action: " << action;

            string worker;

            if (getNextWorker(worker))
            {
                writeLock.lock();

                sendMore(worker);
                send(data);

                writeLock.unlock();
            }
            else
            {
                ERR << "worker not found";
            }
        }

        if (interrupted)
        {
            break;
        }
    }

    serviceThread.join();
}

void broker::dispatchService()
{
    LOG << "Service dispatcher thread started";

    zmq::pollitem_t pollItems[] = {{*service, 0, ZMQ_POLLIN, 0}};

    while (true)
    {
        zmq_poll(pollItems, 1, 1000);

        if (pollItems[0].revents & ZMQ_POLLIN)
        {
            zmq::message_t items[3];
            int            counter   = 0;
            int            more      = 0;
            size_t         more_size = sizeof(more);

            do
            {
                service->recv(&items[counter]);
                service->getsockopt(ZMQ_RCVMORE, &more, &more_size);

                counter++;
            }
            while (more);

            if (counter < 3)
            {
                ERR << "Wrong messages count: " << counter;

                continue;
            }

            string issuer, action;

            issuer = getMessageData(items[0]);
            action = getAction(getMessageData(items[2]));

            if (action == "service.register")
            {
                registerWorker(issuer);
            }
            else if (action == "service.shutdown")
            {
                removeWorker(issuer);

                writeLock.lock();
                sendMore(issuer);
                send("{\"action\":\"shutdown\",\"history\":[],\"issuer\":\"service_queue\",\"data\":[],\"sections\":{}}");
                writeLock.unlock();
            }
            else if (action == "quit")
            {
                interrupted = true;
            }
            else
            {
                ERR << "Unknown service action: " << action;
            }
        }

        if (interrupted)
        {
            break;
        }
    }

    LOG << "Service dispatcher thread finished";
}


broker::broker()
    : currentWorkerIndex(0), connected(false), interrupted(false)
{
}

void broker::connect()
{
    if (connected)
    {
        return;
    }

    ctx = new zmq::context_t();

    input = new zmq::socket_t(*ctx, ZMQ_PULL);
    input->bind("tcp://127.0.0.1:8100");

    output = new zmq::socket_t(*ctx, ZMQ_ROUTER);
    output->bind("tcp://127.0.0.1:8101");

    service = new zmq::socket_t(*ctx, ZMQ_ROUTER);
    service->bind("tcp://127.0.0.1:8102");

    LOG << "Listen:   input on 8100";
    LOG << "Listen:  output on 8101";
    LOG << "Listen: service on 8102";

    connected = true;
}

string broker::getAction(const string &data)
{
    Json::Value  root;   // will contains the root value after parsing.
    Json::Reader reader;

    if (!reader.parse(data, root))
    {
        return "";
    }

    return root.get("action", "").asString();
}

void broker::registerWorker(const string &id)
{
    workersLock.lock();

    bool found = false;

    for (int i = 0; i < workers.size(); i++)
    {
        if (id == workers[i])
        {
            found = true;

            ERR << "Worker already registered: " << id;

            break;
        }
    }

    if (!found)
    {
        workers.push_back(id);

        LOG << "Worker registered: " << id;
    }


    workersLock.unlock();

    waitForWorkers.notify_all();
}

void broker::removeWorker(const string &id)
{
    workersLock.lock();

    vector<string>::iterator it;

    it = find(workers.begin(), workers.end(), id);

    if (it != workers.end())
    {
        // found
        workers.erase(it);
    }

    workersLock.unlock();

    LOG << "Worker unregistered: " << id;
}

void broker::send(const string &data)
{
    send(data, false);
}

void broker::sendMore(const string &data)
{
    send(data, true);
}

void broker::send(const string &data, bool more)
{
    zmq::message_t message(data.size());
    memcpy(message.data(), data.data(), data.size());

    if (more)
    {
        output->send(message, ZMQ_SNDMORE);
    }
    else
    {
        output->send(message);
    }
}

bool broker::getNextWorker(string &workerName)
{
    workersLock.lock();

    while (workers.size() == 0)
    {
        workersLock.unlock();

        LOG << "wait for workers";

        mutex m;
        unique_lock<mutex> lock(m);
        waitForWorkers.wait(lock);

        LOG << "wait for workers: done";

        workersLock.lock();
    }

    if (currentWorkerIndex >= workers.size())
    {
        currentWorkerIndex = 0;
    }

    workerName = workers[currentWorkerIndex];

    ++currentWorkerIndex;

    workersLock.unlock();

    return true;
}

string broker::getMessageData(zmq::message_t &message)
{
    return string(static_cast<char *>(message.data()), message.size());
}

