#include "Struct.h"



extern "C" {

	// �豸��������
	#define DEVICE_NAME L"\\Device\\ProcessDevice"
	//��������
	#define LINK_NAME L"\\DosDevices\\KeProcessDevice"
	
	//��������ʼ��ַ
	#define IRP_IOCTRL_BASE 0x8000
	//����������ɺ꣨�豸���ͣ������룬ͨѶ��ʽ��Ȩ�ޣ�
	#define IRP_IOCTRL_CODE(i) CTL_CODE(FILE_DEVICE_UNKNOWN,IRP_IOCTRL_BASE + i,METHOD_BUFFERED,FILE_ANY_ACCESS)
	//����ͨ�ŵĿ����붨��
	#define CTL_GetProcessCount IRP_IOCTRL_CODE(0)
	#define CTL_GetProcessData IRP_IOCTRL_CODE(1)
	//ȫ�ֱ�����-----------------
	LIST_ENTRY ListHeader = { 0 };	//����ͷ
	DWORD64 ProcessCount = 0;		//���̾������
	//ȫ�ֱ�����-----------------
	// 
	//IRP��ǲ����-Ĭ�ϴ���
	NTSTATUS DispatchCommon(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);
	//IRP��ǲ����-��������ʽ
	NTSTATUS DispatchIoCtrl(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);
	//����������
	NTSTATUS FeatureSearch(ULONG ulCodeFlag[], ULONG64 ulStaAddr, ULONG64 ulEndAddr, PULONG64 lpRetFunAddr);
	//���׻�ȡ PspCidTable ��ַ
	BOOLEAN GetPspCidTableAddr(PULONG64 RetPspCidTableAddr);
	//һ����
	VOID FirstHandleTable(ULONG64 TableBaseAddr, DWORD64 Index1, DWORD64 Index2);
	//������
	VOID SecondHandleTable(ULONG64 TableBaseAddr, DWORD64 Index2);
	//������
	VOID ThirdHandleTable(ULONG64 TableBaseAddr);
	//������
	BOOLEAN AnalysisTable();
	//����������
	NTSTATUS UtilErgodicProcess(PIRP pIrp);
	//��ȡ������������
	VOID UtilGetProcessCompleteName(PEPROCESS pEprocess, PCHAR RetStrBuffer);

	//ж������
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
		//IoStopTimer(pDriverObject->DeviceObject);//����IO��ʱ��
	}


	//������
	NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegPath)
	{
		//�豸����
		UNICODE_STRING uDeviceName = { 0 };
		//������������
		UNICODE_STRING uLinkName = { 0 };
		//״̬
		NTSTATUS ntStatus = STATUS_SUCCESS;
		//�豸����
		PDEVICE_OBJECT pDevicerObject = NULL;
		//��ʼ���ַ���-�豸����
		RtlInitUnicodeString(&uDeviceName, DEVICE_NAME);
		//��ʼ���ַ���-��������
		RtlInitUnicodeString(&uLinkName, LINK_NAME);
		//�����豸����
		ntStatus = IoCreateDevice(pDriverObject, 0, &uDeviceName, FILE_DEVICE_UNKNOWN, 0, TRUE, &pDevicerObject);
		if (!NT_SUCCESS(ntStatus))
		{
			DbgPrint("IoCreateDevice!");
			return ntStatus;
		}

		//���ڻ�������IO��ʽ
		pDevicerObject->Flags |= DO_BUFFERED_IO;

		//������������
		ntStatus = IoCreateSymbolicLink(&uLinkName, &uDeviceName);
		if (!NT_SUCCESS(ntStatus))
		{
			IoDeleteDevice(pDevicerObject);
			DbgPrint("IoCreateSymbolicLink:%x\n", ntStatus);
			return ntStatus;
		}
		//����IO
		pDriverObject->DriverUnload = DriverUnload;
		//���Ĭ�ϴ���
		for (size_t i = 0; i < IRP_MJ_MAXIMUM_FUNCTION + 1; i++)
		{
			pDriverObject->MajorFunction[i] = DispatchCommon;
		}
		pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchIoCtrl;



		//����ʵ��



		return STATUS_SUCCESS;
	}



	//IRP��ǲ����-Ĭ�ϴ���
	NTSTATUS DispatchCommon(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
	{
		//����IRP�����Ѿ��ɹ�
		pIrp->IoStatus.Status = STATUS_SUCCESS;
		//���ض����ֽڵ�����
		pIrp->IoStatus.Information = 0;
		//����IRP��������
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		//�������óɹ�
		return STATUS_SUCCESS;
	}

	//IRP��ǲ����-��������ʽ
	NTSTATUS DispatchIoCtrl(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
	{
		//������
		ULONG uIoCtrlCode = 0;
		//������
		PVOID pBuffer = NULL;
		//����������
		ULONG uBufferLength = 0;
		//IRPջ
		PIO_STACK_LOCATION pStack = NULL;
		//��ȡ������
		pBuffer = pIrp->AssociatedIrp.SystemBuffer;
		//��ȡIRPջ
		pStack = IoGetCurrentIrpStackLocation(pIrp);
		//��ȡ����������
		uBufferLength = pStack->Parameters.DeviceIoControl.InputBufferLength;
		//��ȡ������
		uIoCtrlCode = pStack->Parameters.DeviceIoControl.IoControlCode;
		//���տ�������зַ�
		switch (uIoCtrlCode)
		{
		case CTL_GetProcessCount:
		{
			//��ȡ�����������������
			AnalysisTable();
			ULONG uRetBufferLength = sizeof(DWORD64);
			RtlZeroMemory(pBuffer, uRetBufferLength);
			RtlCopyMemory(pBuffer, &ProcessCount, uRetBufferLength);
			//����IRP�����Ѿ��ɹ�
			pIrp->IoStatus.Status = STATUS_SUCCESS;
			//���ض����ֽڵ�����
			pIrp->IoStatus.Information = uRetBufferLength;
			//����IRP��������
			IoCompleteRequest(pIrp, IO_NO_INCREMENT);
			//�������óɹ�
			return STATUS_SUCCESS;
		}
		case CTL_GetProcessData:
		{
			//�����߳���Ϣ
			return UtilErgodicProcess(pIrp);
		}
		default:
			break;
		}
	}

	//���׻�ȡ PspCidTable ��ַ
	// 
	//win7��PsLookupProcessByProcessId���������� -> PspCidTable
	//win10��PsLookupProcessByProcessId���������� -> PspReferenceCidTableEntry -> PspCidTable
	BOOLEAN GetPspCidTableAddr(PULONG64 RetPspCidTableAddr)
	{
		// ��ȡ PsLookupProcessByProcessId ��ַ
		UNICODE_STRING uFuncName;
		RtlInitUnicodeString(&uFuncName, L"PsLookupProcessByProcessId");
		ULONG64 ulFuncAddr = (ULONG64)MmGetSystemRoutineAddress(&uFuncName);
		if (!ulFuncAddr)
		{
			return FALSE;
		}

		//��ȡPspReferenceCidTableEntry��ַ
		//000000014061D004 E8 87 00 00 00                call    PspReferenceCidTableEntry
		ULONG ulCodeFlag[] = { 0x000087E8,0xD88B4800,0x74C08548,0x6C894835,0x75E83024,0xF7FFA4B6,0x00030C83,0x00000000 };
		FeatureSearch(ulCodeFlag, ulFuncAddr, (ulFuncAddr + 0x80), &ulFuncAddr);
		//CAll��ַ=��Ե�ַ+��һ��ָ��ĵ�ַ
		INT OffsetAddr = *(PULONG64)(ulFuncAddr + 1);
		ULONG64 ReferenceCidTableEntryFuncAddr = ulFuncAddr + OffsetAddr + 5;
		//��ȡPspCidTable
		ULONG64 MovAddr;
		ULONG ulTargetCodeFlag[] = { 0x76058B48,0xF7FFF574,0x0003FCC1,0x487C7400,0x8B48D18B,0x4503E8C8,0x8B48FFFD,0xC08548F8 };
		FeatureSearch(ulTargetCodeFlag, ReferenceCidTableEntryFuncAddr, (ReferenceCidTableEntryFuncAddr + 0x80), &MovAddr);
		//48 8B 0D 00 74 F5 FF          mov     rcx, qword ptr cs:KeNumberProcessorsGroup0+12h
		OffsetAddr = *(PULONG64)(MovAddr + 3);
		*RetPspCidTableAddr = MovAddr + OffsetAddr + 7;
		return TRUE;
	}

	//����������
	//1.ulCodeFlag		�������
	//2.ulStaAddr		��ʼ��ַ
	//3.ulEndAddr		������ַ
	//4.lpRetFunAddr	����ֵ
	NTSTATUS FeatureSearch(ULONG ulCodeFlag[],ULONG64 ulStaAddr, ULONG64 ulEndAddr, PULONG64 lpRetFunAddr)
	{
		//�������
		
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

	//������
	BOOLEAN AnalysisTable()
	{
		//��ʼ������
		InitializeListHead(&ListHeader);
		//��ȡ PspCidTable
		ULONG64 PTR_PspCidTable;
		GetPspCidTableAddr(&PTR_PspCidTable);
		//_HANDLE_TABLE+0x8->TableCode
		ULONG64 TableCode = *(PULONG64)((*(PULONG64)PTR_PspCidTable) + 8);
		//��ȡ��ȼ� ����λΪ��ȼ�
		//��windows10����Ȼ���ö�̬��չ�ķ�������������ٵ�ʱ��Ͳ����²�����ʱ��������в����ϲ��
		//һ�������ŵľ��ǽ��̺��̶߳��󣨼��ܹ��ģ���ҪһЩ���������ܣ������������ŵ���ָ��ĳ��һ�����ָ��
		//ͬ���������ŵ���ָ��������ָ�롣
		INT Tablelevels = TableCode & 3;
		switch (Tablelevels)
		{
		case 0:
		{
			//һ����
			//ʹ��ȫ�־����ʱ����λĨ��
			FirstHandleTable(TableCode & (~3), 0, 0);
			break;
		}
		case 1:
		{
			//������
			SecondHandleTable(TableCode & (~3), 0);
			break;
		}		
		case 2:
		{
			//������
			ThirdHandleTable(TableCode & (~3));
			break;
		}
		default:
			return FALSE;
			break;
		}
		return TRUE;
	}

	//һ����
	VOID FirstHandleTable(ULONG64 TableBaseAddr, DWORD64 Index1, DWORD64 Index2)
	{
		// ����һ����ÿ�������С 16 �������С 4k�����Ա��� 4096/16 = 526 ��
		PEPROCESS pEprocess = NULL;
		//HANDLE Handle = NULL;
		//UNICODE_STRING uProcessTpye;
		//RtlInitUnicodeString(&uProcessTpye, L"Process");
		for (DWORD64 Index = 1; Index < 256; Index++)//Խ����һ��
		{
			if (!MmIsAddressValid((PVOID64)(TableBaseAddr + Index * 16)))
			{
				//DbgPrint("[LYSM] �Ƿ���ַ:%p\n", BaseAddr + i * 16);
				continue;
			}
			//��ȡOBJECT_TYPE
			//POBJECT_TYPE pObjectType = ObGetObjectType((PVOID)ProcessObject);
			//���̾��
			HANDLE ProcessHandle = (HANDLE)(Index * 4 + 1024 * Index1 + 512 * Index2 * 1024);
			//ObjectType->name=Process
			if (NT_SUCCESS(PsLookupProcessByProcessId(ProcessHandle, &pEprocess)))
			{
				INT64 UnDecryptionCode = *(PULONG64)(TableBaseAddr + Index * 0x10);
				// ���� ��ö���
				//v9 = (_BYTE *)((v18[0] >> 0x10) & 0xFFFFFFFFFFFFFFF0ui64);
				PEPROCESS ProcessObject = (PEPROCESS)((UnDecryptionCode >> 0x10) & 0xFFFFFFFFFFFFFFF0);
				PTeListEntry NewNode = (PTeListEntry)MmAllocateNonCachedMemory(sizeof(TeListEntry));
				RtlZeroMemory(NewNode, sizeof(TeListEntry));
				NewNode->ProcessObject = ProcessObject;
				InsertTailList(&ListHeader, &NewNode->Node);//ͷ�巨
				ProcessCount++;
				ObDereferenceObject(pEprocess);
				
			}
			//else if (PsLookupThreadByThreadId(ProcessHandle, &pEporcess) == STATUS_SUCCESS)
			//{
			//	�߳�
			//}
		}
		return;
	}
	
	//������
	VOID SecondHandleTable(ULONG64 TableBaseAddr, DWORD64 Index2)
	{
		// ����������ÿ�������С 8��,���С 4k�����Ա��� 4096/8 = 512 ��
		ULONG64 FirstLevelsTableBaseAddr = 0;
		for (DWORD64 Index = 0; Index < 512; Index++) {
			if (!MmIsAddressValid((PVOID64)(TableBaseAddr + Index * 8)))
			{
				//DbgPrint("[LYSM] �Ƿ�������ָ�루1��:%p\n", BaseAddr + i * 8);
				continue;
			}
			if (!MmIsAddressValid((PVOID64) * (PULONG64)(TableBaseAddr + Index * 8)))
			{
				//DbgPrint("[LYSM] �Ƿ�������ָ�루2��:%p\n", BaseAddr + i * 8);
				continue;
			}
			FirstLevelsTableBaseAddr = *(PULONG64)(TableBaseAddr + Index * 8);
			FirstHandleTable(FirstLevelsTableBaseAddr, Index, Index2);
		}
		return;
	}

	//������
	VOID ThirdHandleTable(ULONG64 TableBaseAddr)
	{
		// ����������ÿ�������С 8��,���С 4k�����Ա��� 4096/8 = 512 ��
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

	//����������
	NTSTATUS UtilErgodicProcess(PIRP pIrp)
	{
		//��ȡ������
		PVOID pBuffer = (PVOID)pIrp->AssociatedIrp.SystemBuffer;
		PPROCESS_INFO pProcessInfo = (PPROCESS_INFO)pBuffer;
		PEPROCESS pEprocess = NULL;

		PLIST_ENTRY pListEntry = NULL;
		pListEntry = ListHeader.Flink;
		for (size_t i = 0; i < ProcessCount; i++)
		{
			//������ȡ�����
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
			//����ڵ�
			pListEntry = pListEntry->Flink;
			RemoveEntryList(&pTeEntry->Node);
			MmFreeNonCachedMemory(pTeEntry, sizeof(TeListEntry));
		}
		ProcessCount = 0;
		pIrp->IoStatus.Status = STATUS_SUCCESS;
		//���ض����ֽڵ�����
		pIrp->IoStatus.Information = (ULONG64)pProcessInfo - (ULONG64)pBuffer;
		//����IRP��������
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		//�������óɹ�
		return STATUS_SUCCESS;
	}

	//��ȡ������������
	VOID UtilGetProcessCompleteName(PEPROCESS pEprocess,PCHAR RetStrBuffer)
	{
		//ͨ��EPROCESS
		//PEPROCESS+0x38->PEB+0x020->ProcessParameters+0x60->ImagePathName 

		PVOID pFilePathName;
		if (!NT_SUCCESS(PsReferenceProcessFilePointer(pEprocess, &pFilePathName)))
		{
			PUCHAR ImageFileName = PsGetProcessImageFileName(pEprocess);
			strcpy(RetStrBuffer, (PCHAR)ImageFileName);
			return;
		}
		PUNICODE_STRING usFilePath = (PUNICODE_STRING)((ULONG64)pFilePathName + 0x58);
		//�ַ�������
		//UNICODE_STRING ת��ANSI_STRING
		ANSI_STRING AnsiProcessPathName = { 0 };
		RtlUnicodeStringToAnsiString(&AnsiProcessPathName, usFilePath, TRUE);
		PCHAR strProcessPathName = (PCHAR)MmAllocateNonCachedMemory(AnsiProcessPathName.MaximumLength);
		strcpy(strProcessPathName, AnsiProcessPathName.Buffer);
		PCHAR pProcessName = strrchr(strProcessPathName, '\\');
		strcpy(RetStrBuffer, (pProcessName + 1));
		//�ͷ��ڴ�
		MmFreeNonCachedMemory(strProcessPathName, AnsiProcessPathName.MaximumLength);
		RtlFreeAnsiString(&AnsiProcessPathName);
		//ExFreePoolWithTag(RetBuffer, 'UPFN');
		return;
	}

}//extern "C"