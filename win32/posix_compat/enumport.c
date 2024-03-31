// Author: https://gaiger-programming.blogspot.com/2015/07/methods-collection-of-enumerating-com.html

#include <windows.h>
#include <setupapi.h>

#include <stdio.h>
#include "enumport.h"

bool EnumerateComPortSetupAPISetupDiClassGuidsFromNamePort(unsigned int *pNumber,
                                                           char *pPortName, int strMaxLen, char *pFriendName)
{
    unsigned int i, jj;
    int ret;

    HMODULE hLibrary;
    char szFullPath[_MAX_PATH];

    GUID *pGuid;
    DWORD dwGuids;
    HDEVINFO hDevInfoSet;

    typedef HKEY(__stdcall SetupDiOpenDevRegKeyFunType)(HDEVINFO, PSP_DEVINFO_DATA, DWORD, DWORD, DWORD, REGSAM);

    typedef BOOL(__stdcall SetupDiClassGuidsFromNameFunType)(LPCTSTR, LPGUID, DWORD, PDWORD);

    typedef BOOL(__stdcall SetupDiEnumDeviceInfoFunType)(HDEVINFO, DWORD, PSP_DEVINFO_DATA);

    typedef HDEVINFO(__stdcall SetupDiGetClassDevsFunType)(LPGUID, LPCTSTR, HWND, DWORD);

    typedef BOOL(__stdcall SetupDiGetDeviceRegistryPropertyFunType)(HDEVINFO, PSP_DEVINFO_DATA, DWORD, PDWORD, PBYTE, DWORD, PDWORD);

    SetupDiOpenDevRegKeyFunType *SetupDiOpenDevRegKeyFunPtr;

    SetupDiClassGuidsFromNameFunType *SetupDiClassGuidsFromNameFunPtr;
    SetupDiGetClassDevsFunType *SetupDiGetClassDevsFunPtr;
    SetupDiGetDeviceRegistryPropertyFunType *SetupDiGetDeviceRegistryPropertyFunPtr;

    SetupDiEnumDeviceInfoFunType *SetupDiEnumDeviceInfoFunPtr;

    BOOL bMoreItems;
    SP_DEVINFO_DATA devInfo;

    ret = FALSE;
    jj = 0;
    szFullPath[0] = '\0';

    // Get the Windows System32 directory

    if (0 == GetSystemDirectory(szFullPath, _countof(szFullPath)))
    {
        printf(TEXT("CEnumerateSerial::UsingSetupAPI1 failed, Error:%lu\n"),
               GetLastError());
        return FALSE;
    } /*if*/

    // Setup the full path and delegate to LoadLibrary
    strcat_s(szFullPath, _countof(szFullPath), "\\");
    strcat_s(szFullPath, _countof(szFullPath), TEXT("SETUPAPI.DLL"));
    hLibrary = LoadLibrary(szFullPath);

    SetupDiOpenDevRegKeyFunPtr =
        (SetupDiOpenDevRegKeyFunType *)GetProcAddress(hLibrary, "SetupDiOpenDevRegKey");

#if defined _UNICODE
    SetupDiClassGuidsFromNameFunPtr = (SetupDiClassGuidsFromNameFunType *)
        GetProcAddress(hLibrary, "SetupDiGetDeviceRegistryPropertyW");
    SetupDiGetClassDevsFunPtr =
        (SetupDiGetClassDevsFunType *)GetProcAddress(hLibrary, "SetupDiGetClassDevsW");
    SetupDiGetDeviceRegistryPropertyFunPtr = (SetupDiGetDeviceRegistryPropertyFunType *)GetProcAddress(hLibrary, "SetupDiGetDeviceRegistryPropertyW");
#else
    SetupDiClassGuidsFromNameFunPtr = (SetupDiClassGuidsFromNameFunType *)
        GetProcAddress(hLibrary, "SetupDiClassGuidsFromNameA");
    SetupDiGetClassDevsFunPtr = (SetupDiGetClassDevsFunType *)
        GetProcAddress(hLibrary, "SetupDiGetClassDevsA");
    SetupDiGetDeviceRegistryPropertyFunPtr = (SetupDiGetDeviceRegistryPropertyFunType *)
        GetProcAddress(hLibrary, "SetupDiGetDeviceRegistryPropertyA");
#endif

    SetupDiEnumDeviceInfoFunPtr = (SetupDiEnumDeviceInfoFunType *)
        GetProcAddress(hLibrary, "SetupDiEnumDeviceInfo");

    // First need to convert the name "Ports" to a GUID using SetupDiClassGuidsFromName
    dwGuids = 0;
    SetupDiClassGuidsFromNameFunPtr(TEXT("Ports"), NULL, 0, &dwGuids);

    if (0 == dwGuids)
        return FALSE;

    // Allocate the needed memory
    pGuid = (GUID *)HeapAlloc(GetProcessHeap(),
                              HEAP_GENERATE_EXCEPTIONS, dwGuids * sizeof(GUID));

    if (NULL == pGuid)
        return FALSE;

    // Call the function again

    if (FALSE == SetupDiClassGuidsFromNameFunPtr(TEXT("Ports"),
                                                 pGuid, dwGuids, &dwGuids))
    {
        return FALSE;
    } /*if*/

    hDevInfoSet = SetupDiGetClassDevsFunPtr(pGuid, NULL, NULL,
                                            DIGCF_PRESENT /*| DIGCF_DEVICEINTERFACE*/);

    if (INVALID_HANDLE_VALUE == hDevInfoSet)
    {
        // Set the error to report
        printf(TEXT("error SetupDiGetClassDevsFunPtr, %ld"), GetLastError());
        return FALSE;
    } /*if */

    // bMoreItems = TRUE;
    devInfo.cbSize = sizeof(SP_DEVINFO_DATA);
    i = 0;
    jj = 0;

    do
    {
        HKEY hDeviceKey;
        BOOL isFound;

        isFound = FALSE;
        bMoreItems = SetupDiEnumDeviceInfoFunPtr(hDevInfoSet, i, &devInfo);
        if (FALSE == bMoreItems)
            break;

        i++;

        hDeviceKey = SetupDiOpenDevRegKeyFunPtr(hDevInfoSet, &devInfo,
                                                DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);

        if (INVALID_HANDLE_VALUE != hDeviceKey)
        {
            size_t nLen;
            LPTSTR pszPortName;

            pszPortName = NULL;

            {
                // First query for the size of the registry value
                DWORD dwType;
                DWORD dwDataSize;
                LONG err;
                DWORD dwAllocatedSize;
                DWORD dwReturnedSize;
                dwType = 0;
                dwDataSize = 0;

                err = RegQueryValueEx(hDeviceKey, TEXT("PortName"), NULL,
                                      &dwType, NULL, &dwDataSize);

                if (ERROR_SUCCESS != err)
                    continue;

                // Ensure the value is a string
                if (dwType != REG_SZ)
                    continue;

                // Allocate enough bytes for the return value
                dwAllocatedSize = dwDataSize + sizeof(char);

                /* +sizeof(char) is to allow us to NULL terminate
                 the data if it is not null terminated in the registry
                */

                pszPortName = (LPTSTR)LocalAlloc(LMEM_FIXED, dwAllocatedSize);

                if (pszPortName == NULL)
                    continue;

                // Recall RegQueryValueEx to return the data
                pszPortName[0] = TEXT('\0');
                dwReturnedSize = dwAllocatedSize;

                err = RegQueryValueEx(hDeviceKey, TEXT("PortName"), NULL,
                                      &dwType, (LPBYTE)pszPortName, &dwReturnedSize);

                if (ERROR_SUCCESS != err)
                {
                    LocalFree(pszPortName);
                    pszPortName = NULL;
                    continue;
                }

                // Handle the case where the data just returned is the same size as the allocated size. This could occur where the data
                // has been updated in the registry with a non null terminator between the two calls to ReqQueryValueEx above. Rather than
                // return a potentially non-null terminated block of data, just fail the method call
                if (dwReturnedSize >= dwAllocatedSize)
                    continue;

                // NULL terminate the data if it was not returned NULL terminated because it is not stored null terminated in the registry
                if (pszPortName[dwReturnedSize / sizeof(char) - 1] != ('\0'))
                    pszPortName[dwReturnedSize / sizeof(char)] = ('\0');
            } /*local varable*/

            // If it looks like "COMX" then
            // add it to the array which will be returned
            nLen = strlen(pszPortName);

            if (3 < nLen)
            {
                if (0 == _strnicmp(pszPortName, TEXT("COM"), 3))
                {
                    if (FALSE == isdigit(pszPortName[3]))
                        continue;

                    // Work out the port number
                    strncpy(pPortName + jj * strMaxLen, pszPortName,
                            strnlen(pszPortName, strMaxLen));

                    //_stprintf_s(&portName[jj][0], strMaxLen, TEXT("%s"), pszPortName);
                }
                else
                {
                    continue;
                } /*if 0 == _strnicmp(pszPortName, TEXT("COM"), 3)*/

            } /*if 3 < nLen*/

            LocalFree(pszPortName);
            isFound = TRUE;

            // Close the key now that we are finished with it
            RegCloseKey(hDeviceKey);
        } /*INVALID_HANDLE_VALUE != hDeviceKey*/

        if (FALSE == isFound)
            continue;

        // If the port was a serial port, then also try to get its friendly name
        {
            char szFriendlyName[1024];
            DWORD dwSize;
            DWORD dwType;
            szFriendlyName[0] = '\0';
            dwSize = sizeof(szFriendlyName);
            dwType = 0;

            if ((TRUE == SetupDiGetDeviceRegistryPropertyFunPtr(hDevInfoSet, &devInfo,
                                                                SPDRP_DEVICEDESC, &dwType, (PBYTE)(szFriendlyName),
                                                                dwSize, &dwSize)) &&
                (REG_SZ == dwType))
            {
                strncpy(pFriendName + jj * strMaxLen, &szFriendlyName[0],
                        strnlen(&szFriendlyName[0], strMaxLen));
            }
            else
            {
                sprintf_s(pFriendName + jj * strMaxLen, strMaxLen, TEXT(""));
            } /*if SetupDiGetDeviceRegistryPropertyFunPtr */
        } /*local variable */

        jj++;
    } while (1);

    HeapFree(GetProcessHeap(), 0, pGuid);

    *pNumber = jj;

    if (0 < jj)
        ret = TRUE;

    return ret;
} /*EnumerateComPortSetupAPISetupDiClassGuidsFromNamePort*/
