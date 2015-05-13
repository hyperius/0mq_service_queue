#ifndef SERVICE_QUEUE_MAIN_HPP
#define SERVICE_QUEUE_MAIN_HPP

#include <boost/log/trivial.hpp>
#include <boost/thread.hpp>
#include <boost/log/attributes/scoped_attribute.hpp>


#define LOG BOOST_LOG_TRIVIAL(info)
#define ERR BOOST_LOG_TRIVIAL(error)

#define WORKER_HB_TIMEOUT  10
#define WORKER_HB_INTERVAL 30

#endif //SERVICE_QUEUE_MAIN_HPP
