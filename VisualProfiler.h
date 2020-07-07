//To be used in conjunction with Chrome Tracing. Simply include this as a header file or add it to your Precompiled Header. Refer to examples below for implementation.

#include <iostream>
#include <string>
#include <cmath>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <thread>
#include <mutex>

#define PROFILING 1
#if PROFILING
#define PROFILE_SCOPE(name) InstrumentationTimer timer##_LINE__(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCSIG__)
#else
#define PROFILE_SCOPE(name)
#endif

struct ProfileResult
{
	const std::string m_Name;
	long long m_ExecutionStart, m_ExecutionEnd;
	uint32_t m_ThreadID;
};

class Instrumentor
{
private:
	std::string m_SessionName = "None";
	std::ofstream m_OutputStream; 
	int m_ProfileCount = 0;
	std::mutex m_Lock;
	bool m_IsSessionActive = false;

	Instrumentor() {}

public:
	static Instrumentor& Get()
	{
		static Instrumentor instance;
		return instance;
	}

	~Instrumentor()
	{
		EndSession();
	}

	void BeginSession(const std::string& name, const std::string& filePath = "results.json")  
	{
		if (m_IsSessionActive)
		{
			EndSession();
		}
		m_IsSessionActive = true;
		m_OutputStream.open(filePath); 
		WriteHeader();
		m_SessionName = name;
	}

	void EndSession()
	{
		if (!m_IsSessionActive)
		{
			return;
		}
		m_IsSessionActive = false;
		WriteFooter();
		m_OutputStream.close();
		m_ProfileCount = 0;
	}

	void WriteProfile(const ProfileResult& result)
	{
		std::lock_guard<std::mutex> lock(m_Lock);

		if (m_ProfileCount++ > 0)
		{
			m_OutputStream << ",";
		}

		std::string name = result.m_Name;
		std::replace(name.begin(), name.end(), '"', '\'');

		m_OutputStream << "{";
		m_OutputStream << "\"cat\":\"function\",";
		m_OutputStream << "\"dur\":" << (result.m_ExecutionEnd - result.m_ExecutionStart) << ',';
		m_OutputStream << "\"name\":\"" << name << "\",";
		m_OutputStream << "\"ph\":\"X\",";
		m_OutputStream << "\"pid\":0,";
		m_OutputStream << "\"tid\":" << result.m_ThreadID << ",";
		m_OutputStream << "\"ts\":" << result.m_ExecutionStart;
		m_OutputStream << "}";

		m_OutputStream.flush(); //Flushing allows us to stream the information into the file in the event the program doesn't end smoothly. It allows us to save the information within in the event of a crash, but can be expensive. Enable if you wish! :) 
	}

	void WriteHeader()
	{
		m_OutputStream << "{\"otherData\": {},\"traceEvents\":[";
		m_OutputStream.flush();
	}

	void WriteFooter()
	{
		m_OutputStream << "]}";
		m_OutputStream.flush();  
	}
};


class InstrumentationTimer
{
private:
	bool m_Stopped;
	std::chrono::time_point<std::chrono::steady_clock> m_StartPoint;
	ProfileResult m_ProfileResult;

public:
	InstrumentationTimer(const std::string name) : m_ProfileResult({ name, 0, 0, 0 }), m_Stopped(false)
	{
		m_StartPoint = std::chrono::high_resolution_clock::now();
	}

	~InstrumentationTimer()
	{
		if (!m_Stopped)
		{
			Stop();
		}
	}

	void Stop()
	{
		auto endTimepoint = std::chrono::high_resolution_clock::now();

		m_ProfileResult.m_ExecutionStart = std::chrono::time_point_cast<std::chrono::microseconds>(m_StartPoint).time_since_epoch().count();
		m_ProfileResult.m_ExecutionEnd = std::chrono::time_point_cast<std::chrono::microseconds>(endTimepoint).time_since_epoch().count();
		m_ProfileResult.m_ThreadID = std::hash<std::thread::id>{}(std::this_thread::get_id());

		Instrumentor::Get().WriteProfile({ m_ProfileResult });
		m_Stopped = true;
	}
};

//---- Benchmarking Example

namespace Benchmarks
{
	void Function1()
	{
		PROFILE_FUNCTION();

		for (int i = 0; i < 1000; i++)
		{
			std::cout << "Hello World #" << i << std::endl;
		}
	}

	void Function2()
	{
		PROFILE_FUNCTION();
		for (size_t i = 0; i < 1000; i++)
		{
			std::cout << "HelloWorld #" << sqrt(i) << std::endl;
		}
	}

	void Function3(int value)
	{
		PROFILE_FUNCTION();
		for (size_t i = 0; i < 1000; i++)
		{
			std::cout << "HelloWorld #" << i + value << std::endl;
		}
	}

	void Function3()
	{
		PROFILE_FUNCTION();
		for (size_t i = 0; i < 1000; i++)
		{
			std::cout << "HelloWorld #" << i << std::endl;
		}
	}

	void RunBenchmarks()
	{
		PROFILE_FUNCTION();
		std::cout << "Running Benchmarks...\n";
		Function1();
		Function2();
		Function3();
		Function3(2);

		//Seperate Threads
		std::thread b([]() { Function2(); });
		std::thread c([]() { Function3(); });
        std::thread d([]() { Function3(3); });		
		b.join();
		c.join();
		d.join();
	}
}

int main()
{
	Instrumentor::Get().BeginSession("Profile"); 
	Benchmarks::RunBenchmarks();
	Instrumentor::Get().EndSession();

	std::cin.get();
}