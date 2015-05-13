#include "broker.hpp"
#include "main.hpp"
#include <json/json.h>
#include <thread>
#include <chrono>
#include <signal.h>
#include <time.h>

using namespace std;

void broker::run()
{
    LOG << "Service queue started";

    signal(SIGINT,  broker::signalHandler);
    signal(SIGTERM, broker::signalHandler);
    signal(SIGHUP,  broker::signalHandler);

    connect();

    thread          serviceThread = thread(&broker::dispatchService, this);
    thread          heartbeatThread = thread(&broker::heartbeat, this);
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

            try
            {
                sendMore(worker);
                send(message);
            }
            catch (zmq::error_t e)
            {
                ERR << "Send faied [" << worker << "]: error " << e.num() << ": " << e.what();
            }

            writeLock.unlock();
        }

        if (interrupted)
        {
            break;
        }
    }

    serviceThread.join();
    heartbeatThread.join();

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

                sendToWorker(issuer, "shutdown");
            }
            else if (action == "pong")
            {
                workerPong(issuer);
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

    LOG << "Shutting down all workers";

    shutdownAllWorkers();

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
        if (id == workers[i].name)
        {
            found = true;

            ERR << "Worker already registered: " << id;

            break;
        }
    }

    if (!found)
    {
        worker_t wrk;

        wrk.name = id;
        wrk.heartbeatSent = 0;
        wrk.lastHeartbitRecieved = 0;

        workers.push_back(wrk);

        LOG << "Worker registered: " << id;
    }


    workersLock.unlock();

    waitForWorkers.notify_all();
}

void broker::removeWorker(const string &id)
{
    workersLock.lock();

    for (vector<worker_t>::iterator it = workers.begin(); it < workers.end(); it++)
    {
        if (id == (*it).name)
        {
            workers.erase(it);

            break;
        }
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

void broker::send(const zmq::message_t &msg, bool more)
{
    int  flags = 0;
    bool result;

    zmq::message_t message(msg.size());
    memcpy(message.data(), msg.data(), msg.size());

    if (more)
    {
        flags = ZMQ_SNDMORE;
    }

    do
    {
        result = output->send(message, flags);
    }
    while (!result); // eagain workaround
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

    workerName = workers[currentWorkerIndex].name;

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

void broker::shutdownAllWorkers()
{
    for (vector<worker_t>::iterator it = workers.begin(); it < workers.end(); it++)
    {
        sendToWorker((*it).name, "shutdown");
    }
}

void broker::heartbeat()
{
    BOOST_LOG_SCOPED_THREAD_TAG("ThreadID", boost::this_thread::get_id());

    LOG << "Heartbit thread started";

    while (true)
    {
        vector<string> toRemove;
        workersLock.lock();

        for (vector<worker_t>::iterator it = workers.begin(); it < workers.end(); it++)
        {
            time_t    now;
            worker_t& worker = *it;

            time(&now);

            if (0 != worker.heartbeatSent && abs(difftime(worker.heartbeatSent, (0 == worker.lastHeartbitRecieved ? now : worker.lastHeartbitRecieved))) > WORKER_HB_TIMEOUT)
            {
                // shutdown worker if heartbeat timed out
                ERR << "Worker shutdown [timeout]: " << worker.name;

                toRemove.push_back(worker.name);

                continue;
            }

            if (worker.heartbeatSent == 0 || difftime(now, worker.heartbeatSent) > WORKER_HB_INTERVAL)
            {
                sendToWorker(worker.name, "ping");
                worker.heartbeatSent = now;

                LOG << "[ping] " << worker.name;
            }
        }

        workersLock.unlock();

        for (vector<string>::iterator it = toRemove.begin(); it < toRemove.end(); it++)
        {
            sendToWorker(*it, "shutdown");
            removeWorker(*it);
        }

        toRemove.clear();

        std::this_thread::sleep_for(std::chrono::seconds(1));

        if (interrupted)
        {
            break;
        }
    }

    LOG << "Heartbit thread finished";
}


void broker::workerPong(const string &id)
{
    workersLock.lock();

    for (vector<worker_t>::iterator it = workers.begin(); it < workers.end(); it++)
    {
        worker_t & worker = *it;

        if (id == worker.name)
        {
            time(&worker.lastHeartbitRecieved);

            LOG << "[pong] " << id << ": " << difftime(worker.lastHeartbitRecieved, worker.heartbeatSent) << " sec";

            break;
        }
    }

    workersLock.unlock();
}

void broker::sendToWorker(const string &id, const string &data)
{
    stringstream ss;

    ss << "{\"action\":\"" << data << "\",\"history\":[],\"issuer\":\"service_queue\",\"data\":[],\"sections\":{}}";

    writeLock.lock();

    try
    {
        sendMore(id);
        send(ss.str());
    }
    catch (zmq::error_t e)
    {
        ERR << "Send faied: error " << e.num() << ": " << e.what();
    }

    writeLock.unlock();
}
