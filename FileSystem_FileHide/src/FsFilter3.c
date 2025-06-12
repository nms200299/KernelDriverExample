/*++

Module Name:

    FsFilter3.c

Abstract:

    �̴����͸� �̿��� ����/���� ���� ��� ����

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

FLT_POSTOP_CALLBACK_STATUS
PostOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
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
    { IRP_MJ_DIRECTORY_CONTROL,
      0,
      NULL,
      PostOperation },
      // �� ���͸� ���� IRP
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
   UserDefined Function.
*************************************************************************/

#define NEXT_LIST           1
#define STOP_LIST           2
#define PREV_TO_NULL        3
#define PREV_TO_NEXT        4


PVOID CmpFileList(
    PWCH CurrentFileNameBuf,
    ULONG CurrentFileNameLen,
    PUNICODE_STRING CmpFileName,
    PVOID PrevListAddr,
    PVOID CurrentAddr,
    ULONG NextListOffset,
    PULONG Flag,
    PNTSTATUS Status
) {
    PVOID NextListAddr = (PVOID)((PUCHAR)CurrentAddr + NextListOffset);


    UNICODE_STRING CurrentFileName;
    CurrentFileName.Buffer = CurrentFileNameBuf;
    CurrentFileName.Length = (USHORT)CurrentFileNameLen;
    CurrentFileName.MaximumLength = CurrentFileName.Length;
    // ���ڿ� �񱳸� ���� UNICODE_STRING ����ü�� ä����

    DbgPrint("[MINIFILTER] CurrentFileName : %wZ", &CurrentFileName);

    if (RtlCompareUnicodeString(&CurrentFileName, CmpFileName, FALSE) == 0) {
            if (PrevListAddr == CurrentAddr) {
            // ������ ��Ұ� �� ó���� ��Ī�ȴ�.
                *Flag = STOP_LIST;
                *Status = STATUS_NO_MORE_ENTRIES;
                DbgPrint("NO_ENTRIES\n");
                return NULL;
                // DirectoryBuffer�� ���� �ּڰ��� ���� �ٲٴ°� �ȵż�, �ƿ� ���� ����.
            } else {
            // ������ ��Ұ� �� ó���� ��Ī�Ǵ� ���� �ƴϴ�. (�߰�, ��)
                if (NextListOffset == 0) {
                    *Flag = PREV_TO_NULL;
                    DbgPrint("PREV_TO_NULL\n");
                    return NULL;
                    // ���� ��Ұ� ������ ���� ����� ���� ������(���� ����� �ּ�)�� NULL ó���Ѵ�.
                    // (ex. A -X-> B(Matched, ������ ���) )
                } else {
                    *Flag = PREV_TO_NEXT;
                    DbgPrint("PREV_TO_NEXT\n");
                    return NextListAddr;
                    // ���� ��Ұ� ������ ���� ����� ���� �������� ������ ���� ����� �ּҸ� �����Ѵ�.
                    // (ex. A -X-> B(Matched, C ��� ����) / A -> C)
                }
            }
    }

    if (NextListOffset == 0) {
        *Flag = STOP_LIST;
        return NULL;
    } // ���� ��Ұ� ������ ��ȸ�� �����.

    *Flag = NEXT_LIST;
    return NextListAddr;
}

VOID IterIdBothDirInfo(PVOID DirBuffer, PUNICODE_STRING CmpFileName, PNTSTATUS Status) {
    PFILE_ID_BOTH_DIR_INFORMATION pCurrentList = (PFILE_ID_BOTH_DIR_INFORMATION)DirBuffer;
    PFILE_ID_BOTH_DIR_INFORMATION PrevListAddr = pCurrentList;
    ULONG Flag = 0; 

    while (Flag != STOP_LIST) {
        PVOID pNextListAddr = CmpFileList(pCurrentList->FileName,
            pCurrentList->FileNameLength,
            CmpFileName,
            (PVOID)PrevListAddr,
            (PVOID)pCurrentList,
            pCurrentList->NextEntryOffset,
            &Flag,
            Status);

        switch (Flag) {
            case PREV_TO_NULL: {
                PrevListAddr->NextEntryOffset = 0;
                return;
                break;
            } // ���� ����Ʈ ������ �׸񿡼� ��Ī�� ��쿡 ���� ó��
            case PREV_TO_NEXT: {
                PrevListAddr->NextEntryOffset = (ULONG)((ULONG_PTR)pNextListAddr - (ULONG_PTR)PrevListAddr);
                return;
                break;
            } // ���� ����Ʈ �߰� �׸񿡼� ��Ī�� ��쿡 ���� ó��
            case NEXT_LIST: {
                PrevListAddr = pCurrentList;
                pCurrentList = pNextListAddr;
                break;
            } // ���� ����Ʈ�� ��ȸ
        }
    }
    return;
}


VOID IterBothDirInfo(PVOID DirBuffer, PUNICODE_STRING CmpFileName, PNTSTATUS Status) {
    PFILE_BOTH_DIR_INFORMATION pCurrentList = (PFILE_BOTH_DIR_INFORMATION)DirBuffer;
    PFILE_BOTH_DIR_INFORMATION PrevListAddr = pCurrentList;
    ULONG Flag = 0;

    while (Flag != STOP_LIST) {
        PVOID pNextListAddr = CmpFileList(pCurrentList->FileName,
            pCurrentList->FileNameLength,
            CmpFileName,
            (PVOID)PrevListAddr,
            (PVOID)pCurrentList,
            pCurrentList->NextEntryOffset,
            &Flag,
            Status);

        switch (Flag) {
        case PREV_TO_NULL: {
            PrevListAddr->NextEntryOffset = 0;
            return;
            break;
        } // ���� ����Ʈ ������ �׸񿡼� ��Ī�� ��쿡 ���� ó��
        case PREV_TO_NEXT: {
            PrevListAddr->NextEntryOffset = (ULONG)((ULONG_PTR)pNextListAddr - (ULONG_PTR)PrevListAddr);
            return;
            break;
        } // ���� ����Ʈ �߰� �׸񿡼� ��Ī�� ��쿡 ���� ó��
        case NEXT_LIST: {
            PrevListAddr = pCurrentList;
            pCurrentList = pNextListAddr;
            break;
        } // ���� ����Ʈ�� ��ȸ
        }
    }
    return;
}

/*************************************************************************
    MiniFilter callback routines.
*************************************************************************/
FLT_POSTOP_CALLBACK_STATUS
PostOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
) {
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    if (KeGetCurrentIrql() > PASSIVE_LEVEL) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    } // IRQL �˻� (PASSIVE_LEVEL�� �ƴ� ���¿��� Paged-Pool �޸𸮿� �������� ���մϴ�)

    NTSTATUS status;
    switch (Data->Iopb->MajorFunction) {
        case IRP_MJ_DIRECTORY_CONTROL: {
        // ���͸� ��� ���� IRP�̰�,
            switch (Data->Iopb->MinorFunction) {
                case IRP_MN_QUERY_DIRECTORY: {
                    // ���͸� ���� ���� ���ǿ� ���� IRP�� ��,
                    if (!NT_SUCCESS(Data->IoStatus.Status)) {
                        return FLT_POSTOP_FINISHED_PROCESSING;
                    } // ��û�� ���� ������ ������ �ƴϸ� return

                    if (Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer == NULL){
                        return FLT_POSTOP_FINISHED_PROCESSING;
                    } // ��ȯ�� ���� ������ ������ return

                    PFLT_FILE_NAME_INFORMATION pCurrentPath = NULL;
                    status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED, &pCurrentPath);
                    if (!NT_SUCCESS(status)) {
                        return FLT_POSTOP_FINISHED_PROCESSING;
                    } // ��û ��ο� ���� ������ ������ ���ϸ� return

                    UNICODE_STRING CmpPath = RTL_CONSTANT_STRING(L"\\DEVICE\\HARDDISKVOLUME*\\");
                    BOOLEAN result = FsRtlIsNameInExpression(&CmpPath, &pCurrentPath->Name, TRUE, NULL);

                    FltReleaseFileNameInformation(pCurrentPath);
                    if (!result) {
                        return FLT_POSTOP_FINISHED_PROCESSING;
                    } // ����̹� ����(��Ƽ��) �ֻ��� ���(ex. C:\)�� ���� ��û�� �ƴϸ� return

                    UNICODE_STRING CmpFileName = RTL_CONSTANT_STRING(L"test.txt");

                    switch (Data->Iopb->Parameters.DirectoryControl.QueryDirectory.FileInformationClass) {
                        case FileIdBothDirectoryInformation: {
                            IterIdBothDirInfo(Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer, &CmpFileName, &Data->IoStatus.Status);
                            break;
                        } // �Ϲ����� UI Ž���⿡�� ���� ������ ��û�� �� ����ϴ� ����ü
                        case FileBothDirectoryInformation: {
                            IterBothDirInfo(Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer, &CmpFileName, &Data->IoStatus.Status);
                            break;
                        } // FindFirstFile() API���� ���� ������ ��û�� �� ����ϴ� ����ü
                        default: {
                            return FLT_POSTOP_FINISHED_PROCESSING;
                            break;
                        }
                    }                   
                    
                    return FLT_POSTOP_FINISHED_PROCESSING;


                    break;
                }
            }
            break;
        }
    }
    return FLT_POSTOP_FINISHED_PROCESSING;
}
// ���� �ڵ带 ����꿡�� �� �������� �� �� �ֵ��� �Ϻη� �ҽ� ������ �и����� �ʾҽ��ϴ�. 
// ���� ��Ź�帳�ϴ�.

