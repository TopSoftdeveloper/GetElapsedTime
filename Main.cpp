#include "DateChecker.h"

//const int ELASPEDDURATION = 10 * SECONDS_IN_A_DAY;
const int ELASPEDDURATION = 60000;
const char FILENAME[256] = "C:\\test.io";
const char TRIGGER[256] = "PT5M";

int main(int argc, char* argv[]) {
	HANDLE hMutex = CreateMutexA(NULL, TRUE, MUTEX_NAME);
	if (hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS)
	{
		return 1;
	}

	const char* exepath = getExePath().c_str();

	if (argc < 2)
	{
		std::cout << "setting schedule" << std::endl;
		MakeSchedule("TimeChecker", TRIGGER, exepath);
	}
	else {
		std::cout << "from schedule" << std::endl;
		MakeSchedule("TimeChecker", "PT5M", exepath);
		RunTimeCheckerAndToAction();
	}
	return 1;
}