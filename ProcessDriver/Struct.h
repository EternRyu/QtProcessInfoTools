#pragma once
#include <ntifs.h>

#ifndef STRUCT
#define STRUCT

extern "C" {
	NTKERNELAPI UCHAR* PsGetProcessImageFileName(PEPROCESS pEprocess);  //不建议使用,查找的是EPROCESS表的ImageFileName[15]大小仅为16字节会自动截断
	NTKERNELAPI struct _PEB* PsGetProcessPeb(PEPROCESS pEprocess);
	NTKERNELAPI HANDLE PsGetProcessInheritedFromUniqueProcessId(PEPROCESS pEprocess);
	NTKERNELAPI POBJECT_TYPE ObGetObjectType(PVOID Object);
	NTKERNELAPI NTSTATUS PsReferenceProcessFilePointer(IN PEPROCESS Process, OUT PVOID* pFilePointer);//获取FILE_OBJECT
	//typedef pobject_type(*obget_object_type)(pvoid object);
	//obget_object_type obgetobjecttype = null;
	//typedef NTSTATUS (*QUERY_INFO_PROCESS)(
	//	_In_            HANDLE           ProcessHandle,
	//	_In_            PROCESSINFOCLASS ProcessInformationClass,
	//	_Out_           PVOID            ProcessInformation,
	//	_In_            ULONG            ProcessInformationLength,
	//	_Out_opt_		PULONG           ReturnLength
	//);
	//QUERY_INFO_PROCESS NtQueryInformationProcess;

	//声明结构

	typedef struct _OBJECT_TYPE
	{
		struct _LIST_ENTRY TypeList;                                            //0x0
		UNICODE_STRING Name;                                            //0x10
		//VOID* DefaultObject;                                                    //0x20
		//UCHAR Index;                                                            //0x28
		//ULONG TotalNumberOfObjects;                                             //0x2c
		//ULONG TotalNumberOfHandles;                                             //0x30
		//ULONG HighWaterNumberOfObjects;                                         //0x34
		//ULONG HighWaterNumberOfHandles;                                         //0x38
		//struct _OBJECT_TYPE_INITIALIZER TypeInfo;                               //0x40
		//struct _EX_PUSH_LOCK TypeLock;                                          //0xb8
		//ULONG Key;                                                              //0xc0
		//struct _LIST_ENTRY CallbackList;                                        //0xc8
	};

	typedef struct _TeListEntry
	{
		PEPROCESS ProcessObject;
		LIST_ENTRY Node;
	}TeListEntry, * PTeListEntry;

	typedef struct _PROCESS_INFO
	{
		CHAR szProcessName[256];
		ULONG64 ulProcessId;
		ULONG64 ulParentProcessId;
		ULONG64 ulPeb;
		ULONG64 ulEprocess;
	}PROCESS_INFO, * PPROCESS_INFO;
}//extern "C"


#endif // !STRUCT