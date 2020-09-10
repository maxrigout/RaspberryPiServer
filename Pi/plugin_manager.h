#pragma once
#ifndef H_PLUGIN_MANAGER
#define H_PLUGIN_MANAGER
#include "server.h"
#include "plugin.h"
#include <vector>
#include <thread>

class PiPluginManager
{
public:
	PiPluginManager(PiServer* p);
	~PiPluginManager();

	int LoadPlugins(std::vector<PiPlugin*> plugs);
	bool AddPlugin(PiPlugin* plug);
	//void NotifyPlugin(int plugin_id, NOTIFICATION id);

	bool Start();
	bool Terminate();
	bool Terminate(int n);

	PiPlugin* operator[](int n) const { return plugins[n]; }
	PiPlugin* at(int n) const { return plugins.at(n); }

	bool IsOnline() { return pserver->IsOnline(); }
	int NbPlugin() { return plugins.size(); }

	std::string ListPlugins();

private:
	PiServer* pserver;
	std::vector<PiPlugin*> plugins;  // vector of plugins
	std::vector<std::thread> thread_plugins; // vector of threads responsible for running the plugins
};
#endif
