#include "plugin.h"

PiPlugin::PiPlugin()
{
	name = "Undefined";
	new_msg = false;
	is_running = false;
	is_initialized = false;
}

PiPlugin::~PiPlugin()
{
	if (is_running)
	{
		Terminate();
	}
}

bool PiPlugin::Init()
{
	is_initialized = Initialize();
	if (is_initialized)
	{
		PluginLog("Plugin initialized");
	}
	else
	{
		PluginLog("Error while initializing plugin");
	}
	return is_initialized;
}

void PiPlugin::Run()
{
	if (!is_initialized)
	{
		is_initialized = Init();
	}
	if (is_initialized)
	{
		is_running = true;
		thread_out = std::thread(&PiPlugin::MsgOut, this);
		while (is_running && pServer->IsOnline())
		{
			std::unique_lock<std::mutex> lock(mu_in);
			if (msg_in.empty())
			{
				cond_in.wait(lock);  // need to figure out the unlock condition function
			}
			if (!msg_in.empty() && is_running) {
				HandleMessage(msg_in.front());
				msg_in.pop();
			}
		}
	}
	else
	{
		PluginLog("Cannot run: Plugin not initialized");
	}
	is_running = false;
}

void PiPlugin::MsgOut() // function running on a new thread that sends the processed messages (msg_out) to the server
{
	while (is_running && pServer->IsOnline())
	{
		std::unique_lock<std::mutex> lock(mu_out);
		if (msg_out.empty())
		{
			cond_out.wait(lock);  // need to figure out the unlock condition function
		}
		if (!msg_out.empty())
		{
			pServer->AddMsgOut(msg_out.front());
			msg_out.pop();
		}
	}
}

//void PiPlugin::Notify(NOTIFICATION type)
//{
//	switch (type)
//	{
//		case NOTIFICATION::SERVER_SHUTDOWN:
//			Terminate();
//			break;
//
//		case NOTIFICATION::NEW_MSG:
//			NewMsg();
//			break;
//
//		default:
//			break;
//	}
//}
//
//void PiPlugin::NewMsg()
//{
//	// add conditional variable
//	// 
//	new_msg = true;
//}

bool PiPlugin::AddMsgIn(client_msg msg) // Add a new message in the queue to be processed
{
	std::unique_lock<std::mutex> lock(mu_in);
	msg_in.emplace(msg);
	lock.unlock();
	cond_in.notify_one();
	return true;
}

bool PiPlugin::AddMsgOut(client_msg msg) // adds a message in the outgoing queue to be sent to the client
{
	std::unique_lock<std::mutex> lock(mu_out);
	msg_out.emplace(msg);
	lock.unlock();
	cond_out.notify_one();
	return true;
}

client_msg PiPlugin::GetMsgIn()
{
	std::unique_lock<std::mutex> lock(mu_in);
	client_msg msg = msg_in.front();
	msg_in.pop();
	return msg;
}

bool PiPlugin::Terminate()
{
	if (is_running) {
		CleanUp();
		is_running = false;
		is_initialized = false;
		cond_in.notify_one();
		cond_out.notify_one();
		thread_out.join();
		return true;
	}
	return false;
}