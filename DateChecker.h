#pragma once
#ifndef __DATE_CHECHER_H_
#define __DATE_CHECHER_H_
	#include <windows.h>
	#include <string>
	#include <iostream>

	#define SECONDS_IN_A_DAY 86400000  // Milliseconds in one day
	#define MUTEX_NAME "TizPowerAppMutex"

	int RunTimeCheckerAndToAction();
	BOOL MakeSchedule(std::string entry, std::string time, std::string targetpath);
	std::string getExePath();

	extern const int ELASPEDDURATION;
	extern const char FILENAME[256];
#endif
