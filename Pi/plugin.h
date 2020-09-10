#pragma once
#ifndef H_PLUGIN
#define H_PLUGIN

#include "server.h"
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

class PiPlugin
{
public:
	PiPlugin();
	virtual ~PiPlugin();

	void Attach(PiServer* s) { pServer = s; }

	bool Init();
	void Run();
	virtual bool Initialize() = 0;
	virtual bool HandleMessage(client_msg msg) = 0;
	virtual bool CleanUp() = 0;
	bool Terminate();

	//void Notify(NOTIFICATION notification_id);
	bool AddMsgIn(client_msg msg); // adds msg to the incoming queue
	bool AddMsgOut(client_msg msg); // adds msg to the outgoing queue
	client_msg GetMsgIn();
	//client_msg GetMsgOut();

	bool IsRunning() const { return is_running; }

	std::string Name() const { return name; }

protected:

	int PluginLog(const char* msg) const { return PluginLog(std::string(msg)); }
	int PluginLog(const std::string& msg) const { return pServer->PiLog(name + ": "+ msg); }

private:
	//void NewMsg();
	void MsgOut(); // takes messages from the outgoing queue, runs on a seperate thread (thread_out)
	//void ThreadMsgOut();

protected:
	std::string name;

private:
	PiServer* pServer;
	std::queue<client_msg> msg_in;
	std::queue<client_msg> msg_out;
	int nb_msg = 0;

	std::thread thread_out;

	std::mutex mu_in;
	std::condition_variable cond_in;
	std::mutex mu_out;
	std::condition_variable cond_out;
	std::thread thread_process;
	bool new_msg;
	bool is_running;
	bool is_initialized;
};
#endif