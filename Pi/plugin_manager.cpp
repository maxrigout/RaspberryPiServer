#include "plugin_manager.h"

PiPluginManager::PiPluginManager(PiServer* p)
{
	pserver = p;
}

PiPluginManager::~PiPluginManager()
{

}

int PiPluginManager::LoadPlugins(std::vector<PiPlugin*> plugs)  // initializes and stores plugins in plugs
{
	int cnt = 0;
	for (auto p : plugs)
	{
		p->Attach(pserver);
		if (p->Init())
		{
			cnt++;
		}
	}
	return cnt;
}

bool PiPluginManager::AddPlugin(PiPlugin* plug)  // adds a plugin and starts it if the server is running
{
	if (plug != nullptr)
	{
		plugins.push_back(plug);
		plug->Attach(pserver);
		plug->Init();
		if (pserver->IsOnline())
		{
			pserver->PiLog("Starting plugin \"" + plug->Name() + "\" id: " + std::to_string(plugins.size()));
			thread_plugins.emplace_back(std::thread(&PiPlugin::Run, plug));
		}
		return true;
	}
	return false;
}

bool PiPluginManager::Start() // starts all plugins
{
	for (int i = 0; i < plugins.size(); i++)
	{
		if (!plugins[i]->IsRunning()) 
		{
			pserver->PiLog("Starting plugin \"" + plugins[i]->Name() + "\" id: " + std::to_string(i));
			thread_plugins.emplace_back(std::thread(&PiPlugin::Run, plugins[i]));
		}
	}
	return true;
}

bool PiPluginManager::Terminate()  // terminates all plugins
{
	for (auto p : plugins)
	{
		p->Terminate();

	}
	pserver->PiLog("waiting for plugin threads to finish");
	for (int i = 0; i < thread_plugins.size(); i++)
	{
		pserver->PiLog(std::to_string(i));
		thread_plugins[i].join();
	}
	pserver->PiLog("threads all done");
	return true;
}

bool PiPluginManager::Terminate(int n) // terminates plugin n
{
	return plugins.at(n)->Terminate();
}

std::string PiPluginManager::ListPlugins()
{
	if (plugins.size() == 0)
	{
		return "no plugins";
	}
	std::string out("");
	for (int i = 0; i < plugins.size(); ++i)
	{
		out += '\n' + std::to_string(i) + " - " + plugins[i]->Name();
	}
	return out;
}