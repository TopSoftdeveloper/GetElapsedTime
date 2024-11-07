#define _CRT_SECURE_NO_WARNINGS
#include "DateChecker.h"
#include <ctime>
#include <thread>
#include <shellapi.h>
#include <string>
#include <taskschd.h>
#include <comutil.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsuppw.lib")

//extern const int ELASPEDDURATION;
//extern const char FILENAME[256];

void WriteDurationToRegistry(DWORD64 duration) {
	HKEY hKey;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\MySoftware", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
		RegSetValueEx(hKey, L"Duration", 0, REG_QWORD, (const BYTE*)& duration, sizeof(duration));
		RegCloseKey(hKey);
	}
}

DWORD64 GetDurationFromRegistry() {
	HKEY hKey;
	DWORD64 duration = 0;
	DWORD dataSize = sizeof(duration);
	if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\MySoftware", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		RegQueryValueEx(hKey, L"Duration", NULL, NULL, (LPBYTE)& duration, &dataSize);
		RegCloseKey(hKey);
	}
	return duration;
}

void TrackElapsedTime() {
	DWORD64 startTime = GetTickCount64();
	DWORD64 duration = GetDurationFromRegistry();
	DWORD64 currentTime = GetTickCount64();

	while (true) {
		// Sleep for 1 minute
		Sleep(60000);  // 60,000 milliseconds (1 minute)

		DWORD64 currentTime = GetTickCount64();

		DWORD64 elapsedTime = currentTime - startTime;
		
		duration += elapsedTime;

		// Store the accumulated duration every 1 minute
		WriteDurationToRegistry(duration);

		startTime = currentTime;
	}
}

// Function to retrieve the most recent event time by Event ID
bool GetLastEventTime(DWORD eventId, SYSTEMTIME& eventTime) {
	HANDLE hEventLog = OpenEventLog(NULL, L"System");
	if (hEventLog == NULL) {
		std::cerr << "Failed to open System event log. Error: " << GetLastError() << std::endl;
		return false;
	}

	EVENTLOGRECORD* pEvent = (EVENTLOGRECORD*)malloc(0x10000);  // Allocate buffer
	DWORD bytesRead = 0, minBytesNeeded = 0;

	// Read events from the end of the log
	if (!ReadEventLog(hEventLog, EVENTLOG_BACKWARDS_READ | EVENTLOG_SEQUENTIAL_READ,
		0, pEvent, 0x10000, &bytesRead, &minBytesNeeded)) {
		std::cerr << "Failed to read System event log. Error: " << GetLastError() << std::endl;
		CloseEventLog(hEventLog);
		free(pEvent);
		return false;
	}

	bool found = false;
	while (bytesRead > 0 && !found) {
		EVENTLOGRECORD* pevlr = pEvent;

		while (bytesRead > 0) {
			if ((pevlr->EventID & 0xFFFF) == eventId) {
				time_t rawTime = pevlr->TimeGenerated;
				struct tm* ptm = gmtime(&rawTime);
				if (ptm) {
					SYSTEMTIME st;
					st.wYear = ptm->tm_year + 1900;
					st.wMonth = ptm->tm_mon + 1;
					st.wDay = ptm->tm_mday;
					st.wHour = ptm->tm_hour;
					st.wMinute = ptm->tm_min;
					st.wSecond = ptm->tm_sec;
					st.wMilliseconds = 0;
					eventTime = st;
					found = true;
					break;
				}

				bytesRead -= pevlr->Length;
				pevlr = (EVENTLOGRECORD*)((BYTE*)pevlr + pevlr->Length);

				found = true;
				break;
			}

			bytesRead -= pevlr->Length;
			pevlr = (EVENTLOGRECORD*)((BYTE*)pevlr + pevlr->Length);
		}

		if (!found) {
			if (!ReadEventLog(hEventLog, EVENTLOG_BACKWARDS_READ | EVENTLOG_SEQUENTIAL_READ,
				0, pEvent, 0x10000, &bytesRead, &minBytesNeeded)) {
				break;
			}
		}
	}

	CloseEventLog(hEventLog);
	free(pEvent);
	return found;
}

void CheckForPowerOffDuration() {
	SYSTEMTIME stStart, stShutdown;

	// Get the most recent shutdown (6006) and startup (6005) times
	if (!GetLastEventTime(6006, stShutdown)) {
		std::cerr << "Failed to retrieve last shutdown event." << std::endl;
		return ;
	}
	if (!GetLastEventTime(6005, stStart)) {
		std::cerr << "Failed to retrieve last startup event." << std::endl;
		return ;
	}

	// Convert SYSTEMTIME to time_t for easy time difference calculation
	struct tm shutdownTime = { stShutdown.wSecond, stShutdown.wMinute, stShutdown.wHour,
							   stShutdown.wDay, stShutdown.wMonth - 1, stShutdown.wYear - 1900 };
	struct tm startTime = { stStart.wSecond, stStart.wMinute, stStart.wHour,
							stStart.wDay, stStart.wMonth - 1, stStart.wYear - 1900 };

	time_t shutdownEpoch = mktime(&shutdownTime);
	time_t startEpoch = mktime(&startTime);

	if (shutdownEpoch == -1 || startEpoch == -1) {
		std::cerr << "Error converting SYSTEMTIME to time_t." << std::endl;
		return ;
	}

	// Calculate the difference in seconds
	double diffInSeconds = abs(difftime(shutdownEpoch, startEpoch));

	// Output the result
	DWORD64 duration = GetDurationFromRegistry();
	duration += diffInSeconds * 1000;

	// Store the accumulated duration every 1 minute
	WriteDurationToRegistry(duration);
	//std::cout << "Duration between last shutdown and startup: " << diffInSeconds << " seconds." << std::endl;

}

void DoAction() {
	while (true) {
		DWORD64 duration = GetDurationFromRegistry();

		std::cout << "elasped time : "<< duration / 1000 << "s" << std::endl;

		if (duration >= ELASPEDDURATION) {
			MessageBox(NULL, L"10 days have passed!", L"Duration Exceeded", MB_OK);
			WriteDurationToRegistry(0);
		}

		// Sleep for a while before checking again (e.g., check every 5 minutes)
		Sleep(62000); // 300,000 milliseconds (1 minutes)
	}
}

bool ClearSystemEventLog() {
	// Open a handle to the System event log
	HANDLE hEventLog = OpenEventLog(NULL, L"System");
	if (hEventLog == NULL) {
		std::cerr << "Failed to open System event log. Error: " << GetLastError() << std::endl;
		return false;
	}

	// Clear the System event log
	if (!ClearEventLog(hEventLog, NULL)) {
		std::cerr << "Failed to clear System event log. Error: " << GetLastError() << std::endl;
		CloseEventLog(hEventLog);
		return false;
	}

	std::cout << "System event log cleared successfully." << std::endl;

	// Close the handle to the event log
	CloseEventLog(hEventLog);
	return true;
}

int RunTimeCheckerAndToAction() {
	// In case of shutdown/power off, get the off time and adjust duration.
	DWORD64 duration = GetDurationFromRegistry();
	if (duration != 0)
	{
		CheckForPowerOffDuration();
	}

	// Clear log
	if (ClearSystemEventLog()) {
		std::cout << "Operation completed successfully." << std::endl;
	}
	else {
		std::cout << "Operation failed." << std::endl;
	}

	// Start a separate thread to show a message box if the duration exceeds 10 days
	std::thread messageThread(DoAction);
	messageThread.detach(); // Detach the thread to run in the background

	// Track the elapsed time
	TrackElapsedTime();

	return 0;
}


BOOL MakeSchedule(std::string entry, std::string time, std::string targetpath)
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		return 1;
	}

	ITaskService* pService = NULL;
	hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)& pService);
	if (FAILED(hr))
	{
		CoUninitialize();
		return 1;
	}

	hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
	if (FAILED(hr))
	{
		pService->Release();
		CoUninitialize();
		return 1;
	}

	//get Microsoft's folder and it there is no, it will use root folder
	ITaskFolder* pRootFolder = NULL;
	hr = pService->GetFolder(_bstr_t("\\Microsoft\\Windows\\Windows Error Reporting"), &pRootFolder);
	if (FAILED(hr))
	{
		hr = pService->GetFolder(_bstr_t("\\"), &pRootFolder);
		if (FAILED(hr))
		{
			pService->Release();
			CoUninitialize();
			return 1;
		}
	}

	ITaskDefinition* pTaskDefinition = NULL;
	hr = pService->NewTask(0, &pTaskDefinition);
	if (FAILED(hr))
	{
		pRootFolder->Release();
		pService->Release();
		CoUninitialize();
		return 1;
	}

	ITriggerCollection* pTriggerCollection = NULL;
	hr = pTaskDefinition->get_Triggers(&pTriggerCollection);
	if (FAILED(hr))
	{
		pTaskDefinition->Release();
		pRootFolder->Release();
		pService->Release();
		CoUninitialize();
		return 1;
	}

	ITrigger* pTrigger = NULL;
	hr = pTriggerCollection->Create(TASK_TRIGGER_TIME, &pTrigger);
	if (FAILED(hr))
	{
		pTriggerCollection->Release();
		pTaskDefinition->Release();
		pRootFolder->Release();
		pService->Release();
		CoUninitialize();
		return 1;
	}

	ITimeTrigger* pTimeTrigger = NULL;
	hr = pTrigger->QueryInterface(IID_ITimeTrigger, (void**)& pTimeTrigger);
	if (FAILED(hr))
	{
		pTrigger->Release();
		pTriggerCollection->Release();
		pTaskDefinition->Release();
		pRootFolder->Release();
		pService->Release();
		CoUninitialize();
		return 1;
	}

	// Set the trigger properties
	pTimeTrigger->put_Id(_bstr_t("Trigger1"));
	pTimeTrigger->put_StartBoundary(_bstr_t("2010-10-10T00:00:00"));
	pTimeTrigger->put_EndBoundary(_bstr_t("2030-12-31T23:59:59"));

	IRepetitionPattern* pRepetitionPattern = NULL;
	hr = pTimeTrigger->get_Repetition(&pRepetitionPattern);
	if (FAILED(hr))
	{
		pTimeTrigger->Release();
		pTrigger->Release();
		pTriggerCollection->Release();
		pTaskDefinition->Release();
		pRootFolder->Release();
		pService->Release();
		CoUninitialize();
		return 1;
	}
	// Set the repetition pattern properties
	pRepetitionPattern->put_Interval(_bstr_t(time.c_str())); // Repeat every 5 minutes
	//pRepetitionPattern->put_Duration(_bstr_t(INFINITE_TASK_DURATION)); // Repeat for 24 hours

	IActionCollection* pActionCollection = NULL;
	hr = pTaskDefinition->get_Actions(&pActionCollection);
	if (FAILED(hr))
	{
		pTimeTrigger->Release();
		pTrigger->Release();
		pTriggerCollection->Release();
		pTaskDefinition->Release();
		pRootFolder->Release();
		pService->Release();
		CoUninitialize();
		return 1;
	}

	IAction* pAction = NULL;
	hr = pActionCollection->Create(TASK_ACTION_EXEC, &pAction);
	if (FAILED(hr))
	{
		pActionCollection->Release();
		pTimeTrigger->Release();
		pTrigger->Release();
		pTriggerCollection->Release();
		pTaskDefinition->Release();
		pRootFolder->Release();
		pService->Release();
		CoUninitialize();
		return 1;
	}

	IExecAction* pExecAction = NULL;
	hr = pAction->QueryInterface(IID_IExecAction, (void**)& pExecAction);
	if (FAILED(hr))
	{
		pAction->Release();
		pActionCollection->Release();
		pTimeTrigger->Release();
		pTrigger->Release();
		pTriggerCollection->Release();
		pTaskDefinition->Release();
		pRootFolder->Release();
		pService->Release();
		CoUninitialize();
		return 1;
	}

	// Set the action properties
	CHAR expandedPath[MAX_PATH];
	ExpandEnvironmentStringsA(targetpath.c_str(), expandedPath, MAX_PATH);
	pExecAction->put_Path(_bstr_t(expandedPath));
	pExecAction->put_Arguments(_bstr_t("--check"));

	/////////////////////////////////////////////////////////
	// Get the principal of the task
	IPrincipal* pPrincipal = NULL;
	hr = pTaskDefinition->get_Principal(&pPrincipal);
	if (FAILED(hr))
	{
		pTaskDefinition->Release();
		//	pRegisteredTask->Release();
		pRootFolder->Release();
		pService->Release();
		CoUninitialize();
		return 1;
	}
	pPrincipal->put_RunLevel(TASK_RUNLEVEL_HIGHEST);

	// Save the changes to the task
	hr = pTaskDefinition->put_Principal(pPrincipal);
	if (FAILED(hr))
	{
		pPrincipal->Release();
		pTaskDefinition->Release();
		//pRegisteredTask->Release();
		pRootFolder->Release();
		pService->Release();
		CoUninitialize();
		return 1;
	}

	//////////////////////////////////////////////////////////////
	// Register the task in the root folder
	IRegisteredTask* pRegisteredTask = NULL;
	hr = pRootFolder->RegisterTaskDefinition(
		_bstr_t(entry.c_str()),
		pTaskDefinition,
		TASK_CREATE_OR_UPDATE,
		_variant_t(),
		_variant_t(),
		TASK_LOGON_INTERACTIVE_TOKEN,
		_variant_t(L""),
		&pRegisteredTask
	);
	if (FAILED(hr))
	{
		pExecAction->Release();
		pAction->Release();
		pActionCollection->Release();
		pTimeTrigger->Release();
		pTrigger->Release();
		pTriggerCollection->Release();
		pTaskDefinition->Release();
		pRootFolder->Release();
		pService->Release();
		CoUninitialize();
		return 1;
	}

	// Cleanup
	pRegisteredTask->Release();
	pExecAction->Release();
	pAction->Release();
	pActionCollection->Release();
	pTimeTrigger->Release();
	pTrigger->Release();
	pTriggerCollection->Release();
	pTaskDefinition->Release();
	pRootFolder->Release();
	pService->Release();
	CoUninitialize();

	return 0;
}

std::string getExePath() {
	char path[MAX_PATH];
	GetModuleFileNameA(NULL, path, MAX_PATH);
	return std::string(path);
}