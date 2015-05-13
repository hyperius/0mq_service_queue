#include "broker.hpp"
#include "main.hpp"
#include <json/json.h>
#include <thread>

using namespace std;

void broker::run()
{
    LOG << "Service queue started";

    signal(SIGINT,  broker::signalHandler);
    signal(SIGTERM, broker::signalHandler);
    signal(SIGHUP,  broker::signalHandler);

    connect();

    thread          serviceThread = thread(&broker::dispatchService, this);
    string          worker;
    zmq::message_t  message;
    zmq::pollitem_t pollItems[]   = {{*input, 0, ZMQ_POLLIN, 0}};

    while (true)
    {
        try
        {
            zmq::poll(pollItems, 1, 1000);
        }
        catch (zmq::error_t e)
        {
        }

        if (pollItems[0].revents & ZMQ_POLLIN)
        {
            input->recv(&message);

            getNextWorker(worker);

            writeLock.lock();

            sendMore(worker);
            send(message);

            writeLock.unlock();
        }

        if (interrupted)
        {
            break;
        }
    }

    serviceThread.join();

    LOG << "Main thread finished";
}

void broker::dispatchService()
{
    BOOST_LOG_SCOPED_THREAD_TAG("ThreadID", boost::this_thread::get_id());

    LOG << "Service dispatcher thread started";

    zmq::pollitem_t pollItems[] = {{*service, 0, ZMQ_POLLIN, 0}};

    while (true)
    {
        try
        {
            zmq_poll(pollItems, 1, 1000);
        }
        catch (zmq::error_t e)
        {
        }

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
    : currentWorkerIndex(0), connected(false), interrupted(false),
      inputDSN("tcp://127.0.0.1:8100"), outputDSN("tcp://127.0.0.1:8101"), serviceDSN("tcp://127.0.0.1:8102")
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
    input->bind(inputDSN.c_str());

    output = new zmq::socket_t(*ctx, ZMQ_ROUTER);
    output->bind(outputDSN.c_str());

    service = new zmq::socket_t(*ctx, ZMQ_ROUTER);
    service->bind(serviceDSN.c_str());

    LOG << "Listen:   input on " << inputDSN;
    LOG << "Listen:  output on " << outputDSN;
    LOG << "Listen: service on " << serviceDSN;

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

void broker::send(const zmq::message_t &msg)
{
    send(msg, false);
}

void broker::sendMore(const zmq::message_t &msg)
{
    send(msg, true);
}

void broker::send(const zmq::message_t &msg, bool more)
{
    zmq::message_t message(msg.size());
    memcpy(message.data(), msg.data(), msg.size());

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

broker* broker::getInstance()
{
    static broker* instance = new broker();

    return instance;
}

void broker::signalHandler(int signal)
{
    ERR << "Signal recieved: " << signal;

    getInstance()->interrupted = true;
}
