// modified by Daniel Deppisch (deppisch@zib.de) from:
// active_msgs.cpp
// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/program_options.hpp>
#include <cstring>
#include <iostream>
#include <fstream>

#include "ham/msg/active_msg_base.hpp"
#include "ham/msg/execution_policy.hpp"
#include "ham/msg/active_msg.hpp"
#include "ham/misc/migratable.hpp"

using namespace std;


namespace ham {

    template<>
    class migratable<std::string> {
    public:
        migratable(const migratable &) = default;

        migratable(migratable &&) = default;

        migratable &operator=(const migratable &) = default;

        migratable &operator=(migratable &&) = default;

        // forward compatible arg into T's ctor
        template<typename Compatible>
        migratable(Compatible &&arg) {
            std::cout << "migratable<string>-ctor: " << arg << std::endl;
            std::strcpy(value, arg.c_str());
        }

        operator std::string() const {
            std::cout << "migratable<string>-conversion: " << value << std::endl;
            return value;
        }

    private:
        char value[256];
    };
} // namespace ham

// a simple message type for testing

class MsgA : public ham::msg::active_msg<MsgA> {
public:
	void operator()() {
                cout << "MsgA::operator() successfully called." << endl;
		// the message could perform some task here
		// and possible send back a result afterwards, e.g. by
		// - using data transferred as member inside the message	
		// - calling some communication layer
		// - ...
	}
	
	// the message could include members that are safe to transfer between the communicating entities
};

class MsgB : public ham::msg::active_msg<MsgB> {
public:
    MsgB(const char* t_in, std::string text2)
    : text2(text2)
    {
        std::strcpy(text, t_in);
    }

    void operator()() {
        cout << "MsgB::operator() successfully called." << endl;
        cout << "Text: " << text << endl;
        cout << "Text2: " << static_cast<std::string>(text2) << endl;
        // the message could perform some task here
        // and possible send back a result afterwards, e.g. by
        // - using data transferred as member inside the message
        // - calling some communication layer
        // - ...
    }
    // the message could include members that are safe to transfer between the communicating entities
private:
    char text[256];
    ham::migratable<string> text2;
};

// a simple test which simulates a communication channel via filesystem
// of course, this does NOT test the communication backend
// this may be used to write and read a message from filesystem to simulate communication between different binaries without a supported backend

// write message to file and shut down
template<typename Msg>
bool write_active_msg(Msg& func, std::string const & filename)
{
        size_t msgSize = sizeof(func);
	

        std::ofstream b_stream(filename.c_str(), std::fstream::out | std::fstream::binary);

        if (b_stream) {
            b_stream.write(reinterpret_cast<char*>(&func), msgSize);
            return (b_stream.good());
        }

        return false;
}

// read message from file and execute
bool read_active_msg(std::string const & filename)
{
    std::ifstream b_stream(filename.c_str(), std::fstream::in | std::fstream::binary);
    b_stream.seekg(0, ios::end);
    int bufferSize = b_stream.tellg();
    char* buffer = new char[bufferSize];
    b_stream.seekg(0, ios::beg);

    if (!b_stream.read(buffer, bufferSize)) {
        cout << "ERROR: reading file " << filename << " failed" << endl;
        return false;
    }

    // simulate reading from the channel, thereby we cast the buffer back to the known base class of all active messages
    auto functor = *reinterpret_cast<ham::msg::active_msg_base*>(buffer);

    // This is where the magick happens.
    // Calling the buffer as an active_msg_base functor with the receive buffer
    // as argument triggers a handler look-up, followed by the execution of
    // that handler (which is defined by the execution policy of the actual
    // message type). The handler can perform a safe upcast of the buffer to
    // the actual type of the message and directly execute it as functor,
    // enqeue it somewhere for further processing, or whatever a policy
    // specifies.
    functor(buffer);

    delete [] buffer;

    return true;
}


int main (int argc, char * argv[]) {

	// initialise active message handler address conversion data
	ham::msg::msg_handler_registry::init();

	// print message registry data
	ham::msg::msg_handler_registry::print_handler_map(std::cout); // generated at static-init-time
	ham::msg::msg_handler_registry::print_handler_vector(std::cout); // generated by the init-call above



    // filename to be used
    std::string filename;
    std::string text;

    // command line handling
    boost::program_options::options_description desc("Options");
        desc.add_options()
          ("file,f", boost::program_options::value<std::string>(&filename), "specify file name (default: \"msgfile\"")
          ("write,w", "make this process write a message to file")
          ("read,r", "make this process read a message from file")
          ("help,h", "print this help information")
          ("text,t", boost::program_options::value<std::string>(&text), "add some text to display when executing message");

    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).allow_unregistered().run(), vm);
    boost::program_options::notify(vm);

    if (!vm.count("file")) {
        filename  = "msgfile";
    }

    // simple message type
    MsgA fA;
    // extended message type
    MsgB fB(text.c_str(), "asdfasdasd");

    if(vm.count("write")) {
        if(vm.count("text")) {
            write_active_msg(fB, filename);
        } else {
            write_active_msg(fA, filename);
        }
    } else if (vm.count("read")) {
        read_active_msg(filename);
    } else {
        cout << "ERROR: did not specify whether process should write or read." << endl;
    }

	return 0;	
}

