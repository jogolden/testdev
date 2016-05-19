// testdev.cpp : Defines the entry point for the application.
//

#include "stdafx.h"

// Credit to c0z for writing files to flash!

DWORD WriteFileToFlash(PBYTE Buffer, LPCSTR Filename, DWORD Length) {
	ANSI_STRING nFlash, fFlash;
	HANDLE hFlashDev, hFlashFile;
	OBJECT_ATTRIBUTES atFlash;
	IO_STATUS_BLOCK ioFlash;
	CHAR destStr[MAX_PATH] = "\\Device\\Flash\\";
	DWORD status, statusNN, dwWritten = 0;
	
	RtlInitAnsiString(&nFlash, "\\Device\\Flash");
	atFlash.RootDirectory = 0;
	atFlash.ObjectName = &nFlash;
	atFlash.Attributes = FILE_ATTRIBUTE_DEVICE;
	
	status = NtOpenFile(&hFlashDev, GENERIC_WRITE | GENERIC_READ | SYNCHRONIZE, &atFlash, &ioFlash, OPEN_EXISTING, FILE_SYNCHRONOUS_IO_NONALERT);
	
	printf("\tNtOpenFile(flash): %08x\n", status);
	
	if(status < 0) {
		return status;
	}

	status = NtDeviceIoControlFile(hFlashDev, 0, 0, 0, &ioFlash, 0x3C8044, 0, 0, 0, 0); // sync/async?
	printf("\tNtDeviceIoControlFile 0x3C8044: %08x\n", status);
	
	if(status == 0) {
		strcat_s(destStr, MAX_PATH, Filename);
		RtlInitAnsiString(&fFlash, destStr);
		atFlash.RootDirectory = 0;
		atFlash.ObjectName = &fFlash;
		atFlash.Attributes = FILE_ATTRIBUTE_DEVICE;
		status = NtCreateFile(&hFlashFile, GENERIC_WRITE | SYNCHRONIZE | FILE_READ_ATTRIBUTES, &atFlash, &ioFlash, 0, 0, FILE_SHARE_READ, TRUNCATE_EXISTING, FILE_SYNCHRONOUS_IO_NONALERT); // TRUNCATE_EXISTING
		printf("\tNtCreateFile: %08x\n", status);
		if(status == 0) {
			while(Length >= 0x4000) {
				status = NtWriteFile(hFlashFile, 0, 0, 0, &ioFlash, (Buffer + dwWritten), 0x4000, 0);
				Length -= 0x4000;
				dwWritten += 0x4000;
				printf("\tNtWriteFile (0x4000): %08x remaining: %x\n", status, Length);				
			}

			if(Length != 0) {
				status = NtWriteFile(hFlashFile, 0, 0, 0, &ioFlash, Buffer + dwWritten, Length, 0);
				dwWritten = dwWritten + Length;
				printf("\tNtWriteFile (0x%x): %08x\n", Length, status);
			}
		}
		statusNN = NtClose(hFlashFile);
		printf("\tNtClose hFlashFile: %08x\n", statusNN);
	}
	
	if(status < 0) {
		statusNN = NtDeviceIoControlFile(hFlashDev, 0, 0, 0, &ioFlash, 0x3C804C, 0, 0, 0, 0); // end sync/async?
		printf("\tNtDeviceIoControlFile 0x3C804C: %08x\n", statusNN);
	} else {
		statusNN = NtDeviceIoControlFile(hFlashDev, 0, 0, 0, &ioFlash, 0x3C8048, 0, 0, 0, 0);// end sync/async and flush?
		printf("\tNtDeviceIoControlFile 0x3C8048: %08x\n", statusNN);
	}
	
	statusNN = NtClose(hFlashDev);
	printf("\tNtClose hFlashDev: %08x\n", statusNN);
	
	return status;
}

BOOL FileExists(PCHAR path) {
	HANDLE file = CreateFile(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file == INVALID_HANDLE_VALUE) {
		return 0;
	}
	CloseHandle(file);
	
	return 1;
}

DWORD ReadFileToBuffer(char* fileName, BYTE* buf, DWORD maxLen) {
	HANDLE file;
	DWORD dwRead = 0;
	file = CreateFileA(fileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file != INVALID_HANDLE_VALUE) {
		ZeroMemory(buf, maxLen);
		ReadFile(file, buf, maxLen, &dwRead, NULL);
		CloseHandle(file);
	}

	return dwRead;
}

BOOL BufferCompare(BYTE* buf1, BYTE* buf2, DWORD len) {
	DWORD i;
	for(i = 0; i < len; i++) {
		if(buf1[i] != buf2[i]) {
			return FALSE;
		}
	}

	return TRUE;
}

#define MAX_SIZE_FLASHFILE 0x10000
BOOL FileCopy(char* fileName, char* targetName) {
	DWORD dwBread, dwFread, sta;
	BOOL res = FALSE;
	BOOL isEqual = FALSE;
	PBYTE filedata = new BYTE[MAX_SIZE_FLASHFILE];

	dwBread = ReadFileToBuffer(fileName, filedata, MAX_SIZE_FLASHFILE);
	if(dwBread != 0){
		char tempName[MAX_PATH] = "Flash:\\";
		strcat_s(tempName, MAX_PATH, targetName);

		BOOL exist = FileExists(tempName);
		if(exist) {
			BYTE* targetdata = new BYTE[MAX_SIZE_FLASHFILE];

			dwFread = ReadFileToBuffer(tempName, targetdata, MAX_SIZE_FLASHFILE);
			
			if(dwFread == dwBread) {
				if(BufferCompare(filedata, targetdata, dwFread)) {
					isEqual = TRUE;
				}
			}
		}

		if(!isEqual) {
			sta = WriteFileToFlash(filedata, targetName, dwBread);
			if(sta == 0) {
				printf("\tsuccess! (wrote %d bytes to %s)\n", dwBread, targetName);
				res = TRUE;
			}
		} else {
			printf("\tfile is already current in flash\n");
		}
	} else {
		printf("\tskipped (error %d trying to read file to buffer)\n", GetLastError());
	}
	delete[] filedata;
	
	return res;
}

void __cdecl main() {
	printf("testdev\n");
	PBYTE pXBDM = 0;
	DWORD dwXBDM = 0;
	XGetModuleSection(GetModuleHandle(NULL), "FFFF0001", (PVOID *)&pXBDM, &dwXBDM);
	printf("attempting to write xbdm.xex to flash...\n");
	if(WriteFileToFlash(pXBDM, "xbdm.xex", dwXBDM) == 0) {
		printf("success! enjoy debugging on your test kit!\n");
		HalReturnToFirmware(HalPowerCycleQuiesceRoutine);
	} else {
		printf("something went wrong...\n");
	}

	/*
	if(FileExists("game:\\xbdm.xex")) {
		if(FileCopy("game:\\xbdm.xex", "xbdm.xex")) {
			printf("success! enjoy debugging on your test kit!\n");
		} else {
			printf("something went wrong...\n");
		}
	}*/
}