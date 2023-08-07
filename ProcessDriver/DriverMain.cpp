#include "Struct.h"



extern "C" {

	// 设备对象名称
	#define DEVICE_NAME L"\\Device\\ProcessDevice"
	//符号链接
	#define LINK_NAME L"\\DosDevices\\KeProcessDevice"
	
	//控制码起始地址
	#define IRP_IOCTRL_BASE 0x8000
	//控制码的生成宏（设备类型，控制码，通讯方式，权限）
	#define IRP_IOCTRL_CODE(i) CTL_CODE(FILE_DEVICE_UNKNOWN,IRP_IOCTRL_BASE + i,METHOD_BUFFERED,FILE_ANY_ACCESS)
	//用于通信的控制码定义
	#define CTL_GetProcessCount IRP_IOCTRL_CODE(0)
	#define CTL_GetProcessData IRP_IOCTRL_CODE(1)
	//全局变量区-----------------
	LIST_ENTRY ListHeader = { 0 };	//链表头
	DWORD64 ProcessCount = 0;		//进程句柄总数
	//全局变量区-----------------
	// 
	//IRP派遣函数-默认处理
	NTSTATUS DispatchCommon(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);
	//IRP派遣函数-控制码形式
	NTSTATUS DispatchIoCtrl(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);
	//特征码搜索
	NTSTATUS FeatureSearch(ULONG ulCodeFlag[], ULONG64 ulStaAddr, ULONG64 ulEndAddr, PULONG64 lpRetFunAddr);
	//简易获取 PspCidTable 地址
	BOOLEAN GetPspCidTableAddr(PULONG64 RetPspCidTableAddr);
	//一级表
	VOID FirstHandleTable(ULONG64 TableBaseAddr, DWORD64 Index1, DWORD64 Index2);
	//二级表
	VOID SecondHandleTable(ULONG64 TableBaseAddr, DWORD64 Index2);
	//三级表
	VOID ThirdHandleTable(ULONG64 TableBaseAddr);
	//解析表
	BOOLEAN AnalysisTable();
	//处理句柄链表
	NTSTATUS UtilErgodicProcess(PIRP pIrp);
	//获取进程完整名称
	VOID UtilGetProcessCompleteName(PEPROCESS pEprocess, PCHAR RetStrBuffer);

	//卸载驱动
	VOID DriverUnload(PDRIVER_OBJECT pDriverObject)
	{
		DbgPrint("Unload Success!");

		if (pDriverObject->DeviceObject)
		{
			UNICODE_STRING SymName = { 0 };
			RtlInitUnicodeString(&SymName, LINK_NAME);
			IoDeleteSymbolicLink(&SymName);
			IoDeleteDevice(pDriverObject->DeviceObject);
		}
		//IoStopTimer(pDriverObject->DeviceObject);//结束IO定时器
	}


	//主程序
	NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegPath)
	{
		//设备名称
		UNICODE_STRING uDeviceName = { 0 };
		//符号链接名称
		UNICODE_STRING uLinkName = { 0 };
		//状态
		NTSTATUS ntStatus = STATUS_SUCCESS;
		//设备对象
		PDEVICE_OBJECT pDevicerObject = NULL;
		//初始化字符串-设备名称
		RtlInitUnicodeString(&uDeviceName, DEVICE_NAME);
		//初始化字符串-符号链接
		RtlInitUnicodeString(&uLinkName, LINK_NAME);
		//创建设备对象
		ntStatus = IoCreateDevice(pDriverObject, 0, &uDeviceName, FILE_DEVICE_UNKNOWN, 0, TRUE, &pDevicerObject);
		if (!NT_SUCCESS(ntStatus))
		{
			DbgPrint("IoCreateDevice!");
			return ntStatus;
		}

		//基于缓冲区的IO方式
		pDevicerObject->Flags |= DO_BUFFERED_IO;

		//创建符号链接
		ntStatus = IoCreateSymbolicLink(&uLinkName, &uDeviceName);
		if (!NT_SUCCESS(ntStatus))
		{
			IoDeleteDevice(pDevicerObject);
			DbgPrint("IoCreateSymbolicLink:%x\n", ntStatus);
			return ntStatus;
		}
		//设置IO
		pDriverObject->DriverUnload = DriverUnload;
		//添加默认处理
		for (size_t i = 0; i < IRP_MJ_MAXIMUM_FUNCTION + 1; i++)
		{
			pDriverObject->MajorFunction[i] = DispatchCommon;
		}
		pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchIoCtrl;



		//功能实现



		return STATUS_SUCCESS;
	}



	//IRP派遣函数-默认处理
	NTSTATUS DispatchCommon(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
	{
		//设置IRP处理已经成功
		pIrp->IoStatus.Status = STATUS_SUCCESS;
		//返回多少字节的数据
		pIrp->IoStatus.Information = 0;
		//结束IRP处理流程
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		//函数调用成功
		return STATUS_SUCCESS;
	}

	//IRP派遣函数-控制码形式
	NTSTATUS DispatchIoCtrl(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
	{
		//控制码
		ULONG uIoCtrlCode = 0;
		//缓冲区
		PVOID pBuffer = NULL;
		//缓冲区长度
		ULONG uBufferLength = 0;
		//IRP栈
		PIO_STACK_LOCATION pStack = NULL;
		//获取缓冲区
		pBuffer = pIrp->AssociatedIrp.SystemBuffer;
		//获取IRP栈
		pStack = IoGetCurrentIrpStackLocation(pIrp);
		//获取缓冲区长度
		uBufferLength = pStack->Parameters.DeviceIoControl.InputBufferLength;
		//获取控制码
		uIoCtrlCode = pStack->Parameters.DeviceIoControl.IoControlCode;
		//按照控制码进行分发
		switch (uIoCtrlCode)
		{
		case CTL_GetProcessCount:
		{
			//获取总数及创建句柄链表
			AnalysisTable();
			ULONG uRetBufferLength = sizeof(DWORD64);
			RtlZeroMemory(pBuffer, uRetBufferLength);
			RtlCopyMemory(pBuffer, &ProcessCount, uRetBufferLength);
			//设置IRP处理已经成功
			pIrp->IoStatus.Status = STATUS_SUCCESS;
			//返回多少字节的数据
			pIrp->IoStatus.Information = uRetBufferLength;
			//结束IRP处理流程
			IoCompleteRequest(pIrp, IO_NO_INCREMENT);
			//函数调用成功
			return STATUS_SUCCESS;
		}
		case CTL_GetProcessData:
		{
			//返回线程信息
			return UtilErgodicProcess(pIrp);
		}
		default:
			break;
		}
	}

	//简易获取 PspCidTable 地址
	// 
	//win7：PsLookupProcessByProcessId（被导出） -> PspCidTable
	//win10：PsLookupProcessByProcessId（被导出） -> PspReferenceCidTableEntry -> PspCidTable
	BOOLEAN GetPspCidTableAddr(PULONG64 RetPspCidTableAddr)
	{
		// 获取 PsLookupProcessByProcessId 地址
		UNICODE_STRING uFuncName;
		RtlInitUnicodeString(&uFuncName, L"PsLookupProcessByProcessId");
		ULONG64 ulFuncAddr = (ULONG64)MmGetSystemRoutineAddress(&uFuncName);
		if (!ulFuncAddr)
		{
			return FALSE;
		}

		//获取PspReferenceCidTableEntry地址
		//000000014061D004 E8 87 00 00 00                call    PspReferenceCidTableEntry
		ULONG ulCodeFlag[] = { 0x000087E8,0xD88B4800,0x74C08548,0x6C894835,0x75E83024,0xF7FFA4B6,0x00030C83,0x00000000 };
		FeatureSearch(ulCodeFlag, ulFuncAddr, (ulFuncAddr + 0x80), &ulFuncAddr);
		//CAll地址=相对地址+下一条指令的地址
		INT OffsetAddr = *(PULONG64)(ulFuncAddr + 1);
		ULONG64 ReferenceCidTableEntryFuncAddr = ulFuncAddr + OffsetAddr + 5;
		//获取PspCidTable
		ULONG64 MovAddr;
		ULONG ulTargetCodeFlag[] = { 0x76058B48,0xF7FFF574,0x0003FCC1,0x487C7400,0x8B48D18B,0x4503E8C8,0x8B48FFFD,0xC08548F8 };
		FeatureSearch(ulTargetCodeFlag, ReferenceCidTableEntryFuncAddr, (ReferenceCidTableEntryFuncAddr + 0x80), &MovAddr);
		//48 8B 0D 00 74 F5 FF          mov     rcx, qword ptr cs:KeNumberProcessorsGroup0+12h
		OffsetAddr = *(PULONG64)(MovAddr + 3);
		*RetPspCidTableAddr = MovAddr + OffsetAddr + 7;
		return TRUE;
	}

	//特征码搜索
	//1.ulCodeFlag		特征码段
	//2.ulStaAddr		起始地址
	//3.ulEndAddr		结束地址
	//4.lpRetFunAddr	返回值
	NTSTATUS FeatureSearch(ULONG ulCodeFlag[],ULONG64 ulStaAddr, ULONG64 ulEndAddr, PULONG64 lpRetFunAddr)
	{
		//特征码段
		
		for (size_t i = ulStaAddr; i < ulEndAddr; i++)
		{
			size_t index = 0;
			while (*(PULONG)(i + (4 * index)) == ulCodeFlag[index])
			{
				index++;
				if (index == 8)
				{
					*lpRetFunAddr = (ULONG64)((PVOID*)i);
					return STATUS_SUCCESS;
				}
			}

		}
		return -1;
	}

	//解析表
	BOOLEAN AnalysisTable()
	{
		//初始化链表
		InitializeListHead(&ListHeader);
		//获取 PspCidTable
		ULONG64 PTR_PspCidTable;
		GetPspCidTableAddr(&PTR_PspCidTable);
		//_HANDLE_TABLE+0x8->TableCode
		ULONG64 TableCode = *(PULONG64)((*(PULONG64)PTR_PspCidTable) + 8);
		//获取表等级 低两位为表等级
		//在windows10中依然采用动态扩展的方法，当句柄数少的时候就采用下层表，多的时候才启用中层表或上层表
		//一级表里存放的就是进程和线程对象（加密过的，需要一些计算来解密），二级表里存放的是指向某个一级表的指针
		//同理三级表存放的是指向二级表的指针。
		INT Tablelevels = TableCode & 3;
		switch (Tablelevels)
		{
		case 0:
		{
			//一级表
			//使用全局句柄表时低两位抹零
			FirstHandleTable(TableCode & (~3), 0, 0);
			break;
		}
		case 1:
		{
			//二级表
			SecondHandleTable(TableCode & (~3), 0);
			break;
		}		
		case 2:
		{
			//三级表
			ThirdHandleTable(TableCode & (~3));
			break;
		}
		default:
			return FALSE;
			break;
		}
		return TRUE;
	}

	//一级表
	VOID FirstHandleTable(ULONG64 TableBaseAddr, DWORD64 Index1, DWORD64 Index2)
	{
		// 遍历一级表（每个表项大小 16 ），表大小 4k，所以遍历 4096/16 = 526 次
		PEPROCESS pEprocess = NULL;
		//HANDLE Handle = NULL;
		//UNICODE_STRING uProcessTpye;
		//RtlInitUnicodeString(&uProcessTpye, L"Process");
		for (DWORD64 Index = 1; Index < 256; Index++)//越过第一项
		{
			if (!MmIsAddressValid((PVOID64)(TableBaseAddr + Index * 16)))
			{
				//DbgPrint("[LYSM] 非法地址:%p\n", BaseAddr + i * 16);
				continue;
			}
			//获取OBJECT_TYPE
			//POBJECT_TYPE pObjectType = ObGetObjectType((PVOID)ProcessObject);
			//进程句柄
			HANDLE ProcessHandle = (HANDLE)(Index * 4 + 1024 * Index1 + 512 * Index2 * 1024);
			//ObjectType->name=Process
			if (NT_SUCCESS(PsLookupProcessByProcessId(ProcessHandle, &pEprocess)))
			{
				INT64 UnDecryptionCode = *(PULONG64)(TableBaseAddr + Index * 0x10);
				// 解密 获得对象
				//v9 = (_BYTE *)((v18[0] >> 0x10) & 0xFFFFFFFFFFFFFFF0ui64);
				PEPROCESS ProcessObject = (PEPROCESS)((UnDecryptionCode >> 0x10) & 0xFFFFFFFFFFFFFFF0);
				PTeListEntry NewNode = (PTeListEntry)MmAllocateNonCachedMemory(sizeof(TeListEntry));
				RtlZeroMemory(NewNode, sizeof(TeListEntry));
				NewNode->ProcessObject = ProcessObject;
				InsertTailList(&ListHeader, &NewNode->Node);//头插法
				ProcessCount++;
				ObDereferenceObject(pEprocess);
				
			}
			//else if (PsLookupThreadByThreadId(ProcessHandle, &pEporcess) == STATUS_SUCCESS)
			//{
			//	线程
			//}
		}
		return;
	}
	
	//二级表
	VOID SecondHandleTable(ULONG64 TableBaseAddr, DWORD64 Index2)
	{
		// 遍历二级表（每个表项大小 8）,表大小 4k，所以遍历 4096/8 = 512 次
		ULONG64 FirstLevelsTableBaseAddr = 0;
		for (DWORD64 Index = 0; Index < 512; Index++) {
			if (!MmIsAddressValid((PVOID64)(TableBaseAddr + Index * 8)))
			{
				//DbgPrint("[LYSM] 非法二级表指针（1）:%p\n", BaseAddr + i * 8);
				continue;
			}
			if (!MmIsAddressValid((PVOID64) * (PULONG64)(TableBaseAddr + Index * 8)))
			{
				//DbgPrint("[LYSM] 非法二级表指针（2）:%p\n", BaseAddr + i * 8);
				continue;
			}
			FirstLevelsTableBaseAddr = *(PULONG64)(TableBaseAddr + Index * 8);
			FirstHandleTable(FirstLevelsTableBaseAddr, Index, Index2);
		}
		return;
	}

	//三级表
	VOID ThirdHandleTable(ULONG64 TableBaseAddr)
	{
		// 遍历三级表（每个表项大小 8）,表大小 4k，所以遍历 4096/8 = 512 次
		ULONG64 SecondLevelsTableBaseAddr = 0;
		for (DWORD64 Index = 0; Index < 512; Index++) {
			if (!MmIsAddressValid((PVOID64)(TableBaseAddr + Index * 8)))
			{ 
				continue;
			}
			if (!MmIsAddressValid((PVOID64) * (PULONG64)(TableBaseAddr + Index * 8)))
			{ 
				continue;
			}
			SecondLevelsTableBaseAddr = *(PULONG64)(TableBaseAddr + Index * 8);
			SecondHandleTable(SecondLevelsTableBaseAddr, Index);
		}

		return;
	}

	//处理句柄链表
	NTSTATUS UtilErgodicProcess(PIRP pIrp)
	{
		//获取缓冲区
		PVOID pBuffer = (PVOID)pIrp->AssociatedIrp.SystemBuffer;
		PPROCESS_INFO pProcessInfo = (PPROCESS_INFO)pBuffer;
		PEPROCESS pEprocess = NULL;

		PLIST_ENTRY pListEntry = NULL;
		pListEntry = ListHeader.Flink;
		for (size_t i = 0; i < ProcessCount; i++)
		{
			//链表中取出句柄
			PTeListEntry pTeEntry = CONTAINING_RECORD(pListEntry, TeListEntry, Node);
			pProcessInfo->ulProcessId = (ULONG64)PsGetProcessId(pTeEntry->ProcessObject);
			pProcessInfo->ulParentProcessId = (ULONG64)PsGetProcessInheritedFromUniqueProcessId(pTeEntry->ProcessObject);
			pProcessInfo->ulPeb = (ULONG64)PsGetProcessPeb(pTeEntry->ProcessObject);
			pProcessInfo->ulEprocess = (ULONG64)pTeEntry->ProcessObject;
			//PUCHAR ImageFileName = PsGetProcessImageFileName(pTeEntry->ProcessObject);
			//strcpy(pProcessInfo->szProcessName, (PCHAR)ImageFileName);
			UtilGetProcessCompleteName(pTeEntry->ProcessObject, pProcessInfo->szProcessName);
			pProcessInfo++;
			//ObDereferenceObject(pEprocess);
			//清除节点
			pListEntry = pListEntry->Flink;
			RemoveEntryList(&pTeEntry->Node);
			MmFreeNonCachedMemory(pTeEntry, sizeof(TeListEntry));
		}
		ProcessCount = 0;
		pIrp->IoStatus.Status = STATUS_SUCCESS;
		//返回多少字节的数据
		pIrp->IoStatus.Information = (ULONG64)pProcessInfo - (ULONG64)pBuffer;
		//结束IRP处理流程
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		//函数调用成功
		return STATUS_SUCCESS;
	}

	//获取进程完整名称
	VOID UtilGetProcessCompleteName(PEPROCESS pEprocess,PCHAR RetStrBuffer)
	{
		//通过EPROCESS
		//PEPROCESS+0x38->PEB+0x020->ProcessParameters+0x60->ImagePathName 

		PVOID pFilePathName;
		if (!NT_SUCCESS(PsReferenceProcessFilePointer(pEprocess, &pFilePathName)))
		{
			PUCHAR ImageFileName = PsGetProcessImageFileName(pEprocess);
			strcpy(RetStrBuffer, (PCHAR)ImageFileName);
			return;
		}
		PUNICODE_STRING usFilePath = (PUNICODE_STRING)((ULONG64)pFilePathName + 0x58);
		//字符串处理
		//UNICODE_STRING 转换ANSI_STRING
		ANSI_STRING AnsiProcessPathName = { 0 };
		RtlUnicodeStringToAnsiString(&AnsiProcessPathName, usFilePath, TRUE);
		PCHAR strProcessPathName = (PCHAR)MmAllocateNonCachedMemory(AnsiProcessPathName.MaximumLength);
		strcpy(strProcessPathName, AnsiProcessPathName.Buffer);
		PCHAR pProcessName = strrchr(strProcessPathName, '\\');
		strcpy(RetStrBuffer, (pProcessName + 1));
		//释放内存
		MmFreeNonCachedMemory(strProcessPathName, AnsiProcessPathName.MaximumLength);
		RtlFreeAnsiString(&AnsiProcessPathName);
		//ExFreePoolWithTag(RetBuffer, 'UPFN');
		return;
	}

}//extern "C"