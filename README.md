0MQ Service Queue
=================

Replacement for default zmq_device with true round-robin and heartbeat

---

How to build
============

Run this commands to build project:

```bash
$ cd /path/to/sources
$ mkdir build
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ../
$ make
```

Usage
=====

```bash
$ cd /path/to/build/folder
$ cp -R default config_name
$ nano config_name/config.json
$ ./service_queue config_name
```

Dependencies
============
* [libzmq 4.x](https://github.com/zeromq/zeromq4-x)
* [libjsoncpp](https://github.com/open-source-parsers/jsoncpp)
* [boost](http://www.boost.org/): thread, system, log, filesystem