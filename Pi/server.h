#pragma once

#define UNIX


#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstdint>
#include <cstring> // for memset

#ifdef UNIX
// add the -lpthread option to g++
#include <sys/types.h> 
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

#define SOCKET_ERROR -1

#else
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#define close closesocket
#endif

#define MAX_PACKET_SZ 40960
#define MIN_PACKET_SZ 9
#define RASPBIAN_MAX_RECV_SZ 1461
#define MAX_NB_CLIENT 1000
#define DEFAULT_PORT 27015


#define START_DATA '\2' // start data character
#define END_DATA '\3'   // end data character

#define ACK_CHAR '\6' // acknowledgement character
#define DIS_CHAR '\4' // disconnected character

class PiPluginManager;
class PiPlugin;

struct client_info
{
	int socket;
	std::string ip;
	int id;
	int plugin; // tells to which plugin the client is connected to
};

struct client_msg
{
	client_msg(client_info cl)
	{
		client = cl;
		header = "";
		data = "";
	}

	client_msg(client_info cl, std::string h, std::string d)
	{
		client = cl;
		header = h;
		data = d;
	}

	client_msg(const client_msg& old) 
	{
		client = old.client;
		header = old.header;
		data = old.data;
	}

	template <typename t>
	static std::string ToHex(t n) {
		std::string hexTable = "0123456789ABCDEF";
		std::string out(sizeof(t) * 2, '0');
		t n2 = n;
		for (int i = out.length(); i > 0; i--) {
			out[i - 1] = hexTable[(n2 & 0xf)];
			n2 = n2 >> 4;
		}
		return out;
	}

	std::string MakeHeader() const
	{
		return START_DATA + ToHex<uint16_t>(MIN_PACKET_SZ + header.length() + data.length() + 1) + ToHex<uint16_t>(header.length()) + header;
	}

	std::string Pack() const
	{
		return MakeHeader() + data + END_DATA;
	}

	void Unpack(std::string msg)
	{
		//msg with type
		// b-pppp-hhhh-header-data-e
		// ------------ -> discard all those characters along with ending e
		std::string new_msg(msg.substr(MIN_PACKET_SZ, msg.length() - 1 - MIN_PACKET_SZ));
		int header_sz = stoi(msg.substr(5, 4), nullptr, 16);
		header = msg.substr(MIN_PACKET_SZ, header_sz);
		data = new_msg.substr(header_sz, std::string::npos);
	}

	client_info client;
	std::string header;
	std::string data;
};

enum NOTIFICATION
{
	NEW_MSG = 1,
	SERVER_SHUTDOWN,
};

class PiServer
{
public:

	PiServer();
	~PiServer();

	int Init(int port);
	int Init(std::string port) { return Init(stoi(port)); }

	int PiLog(const char* msg) { return PiLog(std::string(msg)); }
	int PiLog(const std::string& msg);

	int Start();
	int Shutdown();

	void Command();
	void Help();
	void UnkownCommand();
	void TestMessage();
	void ShowIP();
	void ShowPort();
	void ListPlugins();

	void AddMsgIn(const client_msg& msg);
	void AddMsgOut(const client_msg& msg);
	//client_msg GetMsgIn();
	//client_msg GetMsgOut();
	void Thread_DispatchMessage();
	void Thread_SendMessage();

	int CloseConnection(client_info* c);


	// plugin manager
	int LoadPlugins(std::vector<PiPlugin*> plug);
	bool AddPlugin(PiPlugin* plug);
	//void NotifyPlugin(int plugin_id, NOTIFICATION id);

	bool const IsOnline() const { return is_online; }


private:
	void ReceiveMessage(client_info* socket);
	void ReceiveMsg_test(client_msg m);
	int Listen();
	int Accept();

	bool SendTo(int socket, std::string msg);
	bool SendMessage(const client_msg& msg);
	std::string RecvStrFrom(int so, int sz);

private:
	int port;
	std::string ip;

	int socket_listen;
	std::thread thread_listen; // thread responsible for accepting new connections
	std::thread thread_dispatch; // thread responsible for dispatching the incoming messages (msg_in) to the correct plugin
	std::thread thread_send; // thread responsible for sending msg (msg_out) to the clients

	sockaddr_in serverAddress;
	sockaddr_in clientAddress;

	std::map<int, std::thread> client_threads;
	std::map<int, client_info*> client_sockets;

	PiPluginManager* plugin_manager;

	std::queue<client_msg> queue_in;
	std::queue<client_msg> queue_out;

	bool is_ready;
	bool is_online;
	bool is_listening;

	bool console_log;

	int nb_clients;
	int nb_connection;
	uint64_t nb_msg_in;
	uint64_t nb_msg_out;

	std::mutex mu_in;
	std::mutex mu_out;
	std::mutex mu_log;
	std::condition_variable cond_in;
	std::condition_variable cond_out;

	// virtual void ProcessRequest() = 0;
};
#endif