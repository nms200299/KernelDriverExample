/*++

Module Name:

    FsFilter3.c

Abstract:

    �̴����͸� �̿��� MBR ��ȣ ��� ����

Environment:

    Kernel mode

--*/

#include <fltKernel.h>
#include <dontuse.h>

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")


PFLT_FILTER gFilterHandle;


/*************************************************************************
    Prototypes
*************************************************************************/

EXTERN_C_START

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    );

NTSTATUS
FsFilterUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
PreOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    );

EXTERN_C_END

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, FsFilterUnload)
#endif

//
//  operation registration
//

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
    { IRP_MJ_WRITE,
      0,
      PreOperation,
      NULL },
      // �� ���� ������ ���� IRP

      { IRP_MJ_OPERATION_END }
};

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {
    sizeof( FLT_REGISTRATION ),         //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags
    NULL,                               //  Context
    Callbacks,                          //  Operation callbacks
    FsFilterUnload,                    //  MiniFilterUnload 
    NULL,                               //  InstanceSetup
    NULL,                               //  InstanceQueryTeardown
    NULL,                               //  InstanceTeardownStart
    NULL,                               //  InstanceTeardownComplete
    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent
};


/*************************************************************************
    MiniFilter initialization and unload routines.
*************************************************************************/

NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER( RegistryPath );

    status = FltRegisterFilter( DriverObject,
                                &FilterRegistration,
                                &gFilterHandle );

    FLT_ASSERT( NT_SUCCESS( status ) );

    if (NT_SUCCESS( status )) {
        status = FltStartFiltering( gFilterHandle );

        if (!NT_SUCCESS( status )) {
            FltUnregisterFilter( gFilterHandle );
        }
    }

    return status;
}

NTSTATUS
FsFilterUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    )
{
    UNREFERENCED_PARAMETER( Flags );
    PAGED_CODE();

    FltUnregisterFilter( gFilterHandle );

    return STATUS_SUCCESS;
}


/*************************************************************************
    ETC...
*************************************************************************/
BOOLEAN TerminateProcess(HANDLE pid) {
    HANDLE hProcess = NULL;
    OBJECT_ATTRIBUTES objAttr;
    CLIENT_ID clientId;

    clientId.UniqueProcess = pid;
    clientId.UniqueThread = NULL;
    InitializeObjectAttributes(&objAttr, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

    NTSTATUS status = ZwOpenProcess(&hProcess, PROCESS_ALL_ACCESS, &objAttr, &clientId);
    if (!NT_SUCCESS(status)) {
        return FALSE;
    }
    status = ZwTerminateProcess(hProcess, STATUS_ACCESS_DENIED);
    ZwClose(hProcess);
    if (!NT_SUCCESS(status)) {
        return FALSE;
    }
    return TRUE;
} // ���μ��� ���� �Լ�

/*************************************************************************
    MiniFilter callback routines.
*************************************************************************/
FLT_PREOP_CALLBACK_STATUS
PreOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    )

{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    if (KeGetCurrentIrql() > PASSIVE_LEVEL) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    } // IRQL �˻� (PASSIVE_LEVEL�� �ƴ� ���¿��� Paged-Pool �޸𸮿� �������� ���մϴ�)

    NTSTATUS status;

    switch (Data->Iopb->MajorFunction) {
        case IRP_MJ_WRITE: {
            PFLT_FILE_NAME_INFORMATION pFileInfo = NULL;

            status = FltGetFileNameInformation(Data, FLT_FILE_NAME_OPENED, &pFileInfo);
            if ((!NT_SUCCESS(status)) || (pFileInfo == NULL)) {
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            } // ���� ��θ� �� �� ������ �ش� IRP�� ����մϴ�. (PostOperation �̼���)

            UNICODE_STRING CmpDiskPath1 = RTL_CONSTANT_STRING(L"\\DEVICE\\HARDDISK*\\DR*");
            BOOLEAN result = FsRtlIsNameInExpression(&CmpDiskPath1, &pFileInfo->Name, TRUE, NULL);
            // "\\DEVICE\\HARDDISK*\\DR*" ��θ� ��

            if (!result) {
                UNICODE_STRING CmpDiskPath2 = RTL_CONSTANT_STRING(L"\\DEVICE\\HARDDISK*\\PARTITION0");
                result = FsRtlIsNameInExpression(&CmpDiskPath2, &pFileInfo->Name, TRUE, NULL);
            } // "\\DEVICE\\HARDDISK*\\PARTITION0" ��θ� ��

            FltReleaseFileNameInformation(pFileInfo);
            // ���� �̸� �Ҵ� ����

            if ((result) && (Data->Iopb->Parameters.Write.ByteOffset.QuadPart <= 512)) {
                // �� ��ο��� 512 ����Ʈ ������ ���ο� �����Ѵٸ�, MBR �������� ���� 
                ULONG AccessPID = FltGetRequestorProcessId(Data);
                DbgPrint("[MINIFILTER] MBR Write Detection. PID(%lu) Path(%wZ)\n", (ULONG)(ULONG_PTR)AccessPID, &pFileInfo->Name);

                //if (NT_SUCCESS(status)) {
                //    if (TerminateProcess((HANDLE)(ULONG_PTR)AccessPID)) {
                //        DbgPrint("[MINIFILTER] Success to terminate process. (%lu)\n", (ULONG)(ULONG_PTR)AccessPID);
                //    } else {
                //        DbgPrint("[MINIFILTER] Failed to terminate process. (%lu)\n", (ULONG)(ULONG_PTR)AccessPID);
                //    } // EPROCESS ��ü�� �̿��� ���μ����� �����ŵ�ϴ�.
                //} // PID�� ���� EPROCESS ��ü�� �Ҵ�޽��ϴ�.

                Data->IoStatus.Status = STATUS_ACCESS_DENIED;
                Data->IoStatus.Information = 0;
                return FLT_PREOP_COMPLETE;
                // MBR �������� ���ֵǸ�, IRP�� �ź��մϴ�.
            }
            break;
        }
    }
 
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
    // �� ���ǿ� �ش����� ������ IRP�� ����մϴ�.
}