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


#define IPR_TO_NULL         1
#define IPR_TO_NEXT         2
#define PREV_TO_NULL        3
#define PREV_TO_NEXT        4
#define NEXT_LIST           5
#define STOP_LIST           6

PVOID CmpFileList(
    PWCH CurrentFileNameBuf,
    ULONG CurrentFileNameLen,
    PUNICODE_STRING CmpFileName,
    PVOID PrevListAddr,
    PVOID CurrentAddr,
    ULONG NextListOffset,
    PULONG Flag
) {
    PVOID NextListAddr = (PVOID)((PUCHAR)CurrentAddr + NextListOffset);


    UNICODE_STRING CurrentFileName;
    CurrentFileName.Buffer = CurrentFileNameBuf;
    CurrentFileName.Length = (USHORT)CurrentFileNameLen;
    CurrentFileName.MaximumLength = CurrentFileName.Length;
    // ���ڿ� �񱳸� ���� UNICODE_STRING ����ü�� ä����

    DbgPrint("[MINIFILTER] CurrentFileName : %wZ", &CurrentFileName);

    if (RtlCompareUnicodeString(&CurrentFileName, CmpFileName, TRUE) == 0) {
            if (PrevListAddr == CurrentAddr) {
                if (NextListOffset == 0) {
                    *Flag = IPR_TO_NULL;
                    return NULL;
                } else {
                    *Flag = IPR_TO_NEXT;
                    return NextListAddr;
                }
            } else {
                if (NextListOffset == 0) {
                    *Flag = PREV_TO_NULL;
                    return NULL;
                }
                else {
                    *Flag = PREV_TO_NEXT;
                    return NextListAddr;
                }
            }
    }

    if (NextListOffset == 0) {
        *Flag = STOP_LIST;
        return NULL;
    }

    *Flag = NEXT_LIST;
    return NextListAddr;
}


PVOID IterDirInfo(PVOID DirBuffer, PUNICODE_STRING CmpFileName) {
    PFILE_DIRECTORY_INFORMATION pCurrentList = (PFILE_DIRECTORY_INFORMATION)DirBuffer;
    PFILE_DIRECTORY_INFORMATION PrevListAddr = pCurrentList;
    ULONG Flag = 0;

    while (Flag != STOP_LIST) {
        PVOID pNextListAddr = CmpFileList(pCurrentList->FileName,
            pCurrentList->FileNameLength,
            CmpFileName,
            (PVOID)PrevListAddr,
            (PVOID)pCurrentList,
            pCurrentList->NextEntryOffset,
            &Flag);

        switch (Flag){
            case IPR_TO_NULL: {
                pCurrentList = NULL;
                return NULL;
                break;
            }
            case IPR_TO_NEXT: {
                return pNextListAddr;
                break;
            }
            case PREV_TO_NULL: {
                PrevListAddr->NextEntryOffset = 0;
                return DirBuffer;
                break;
            }
            case PREV_TO_NEXT: {
                PrevListAddr->NextEntryOffset = (ULONG)((PUCHAR)pNextListAddr - (PUCHAR)PrevListAddr);
                break;
            }
            case NEXT_LIST: {
                PrevListAddr = pCurrentList;
                pCurrentList = pNextListAddr;
                break;
            }
        }
    }
    return DirBuffer;
}

//IterNameInfo(PVOID DirectoryBuffer) {
//
//}
//
PVOID IterIdBothDirInfo(PVOID DirBuffer, PUNICODE_STRING CmpFileName) {
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
            &Flag);

        switch (Flag) {
            case IPR_TO_NULL: {
                pCurrentList = NULL;
                return NULL;
                break;
            }
            case IPR_TO_NEXT: {
                return pNextListAddr;
                break;
            }
            case PREV_TO_NULL: {
                PrevListAddr->NextEntryOffset = 0;
                return DirBuffer;
                break;
            }
            case PREV_TO_NEXT: {
                PrevListAddr->NextEntryOffset = (ULONG)((PUCHAR)pNextListAddr - (PUCHAR)PrevListAddr);
                return DirBuffer;
                break;
            }
            case NEXT_LIST: {
                PrevListAddr = pCurrentList;
                pCurrentList = pNextListAddr;
                break;
            }
        }
    }
    return DirBuffer;
}

//IterIdFullDirInfo(PVOID DirectoryBuffer) {
//
//}
//
//IterBothDirInfo(PVOID DirectoryBuffer) {
//
//}
//
//IterFullDirInfo(PVOID DirectoryBuffer) {
//
//}


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

                   

                    UNICODE_STRING CmpPath = RTL_CONSTANT_STRING(L"\\DEVICE\\HARDDISKVOLUME*\\TEST");
                    BOOLEAN result = FsRtlIsNameInExpression(&CmpPath, &pCurrentPath->Name, TRUE, NULL);

                    FltReleaseFileNameInformation(pCurrentPath);
                    if (!result) {
                        return FLT_POSTOP_FINISHED_PROCESSING;
                    } // ����̹� ����(��Ƽ��) �ֻ��� ���(ex. C:\)�� ���� ��û�� �ƴϸ� return

                    UNICODE_STRING CmpFileName = RTL_CONSTANT_STRING(L"TEST.TXT");

                    switch (Data->Iopb->Parameters.DirectoryControl.QueryDirectory.FileInformationClass) {
                        case FileDirectoryInformation: {
                            Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer =
                                IterDirInfo(Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer, &CmpFileName);
                            break;
                        }
                        //case FileNamesInformation: {
                        //    PFILE_NAMES_INFORMATION pCurrentList = (PFILE_NAMES_INFORMATION)Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;
                        //    break;
                        //}
                        case FileIdBothDirectoryInformation: {
                            Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer =
                                IterIdBothDirInfo(Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer, &CmpFileName);
                            break;
                        } // �Ϲ����� UI Ž���⿡�� ���� ������ ��û�� �� ����ϴ� ����ü
                        //case FileIdFullDirectoryInformation: {
                        //    PFILE_ID_FULL_DIR_INFORMATION pCurrentList = (PFILE_ID_FULL_DIR_INFORMATION)Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;
                        //    break;
                        //}
                        //case FileBothDirectoryInformation: {
                        //    PFILE_BOTH_DIR_INFORMATION pCurrentList = (PFILE_BOTH_DIR_INFORMATION)Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;
                        //    break;
                        //} // FindFirstFile() API���� ���� ������ ��û�� �� ����ϴ� ����ü
                        //case FileFullDirectoryInformation: {
                        //    PFILE_FULL_DIR_INFORMATION pCurrentList = (PFILE_FULL_DIR_INFORMATION)Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;
                        //    break;
                        //}
                        default: {
                            return FLT_POSTOP_FINISHED_PROCESSING;
                            break;
                        }
                    }
                    //Data->IoStatus.Status = STATUS_NO_MORE_ENTRIES;
                   
                    
                    return FLT_POSTOP_FINISHED_PROCESSING;


                    break;
                }
            }
            break;
        }
    }
    return FLT_POSTOP_FINISHED_PROCESSING;
}
// ���� �ڵ��� �б⹮���� �Ϻη� �Լ�ȭ ��Ű�� �ʾҽ��ϴ�.
// ���� ��Ź�帳�ϴ�.

