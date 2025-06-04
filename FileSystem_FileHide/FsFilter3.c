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

                    DbgPrint("[MINIFILTER] DirQuery : %wZ", &pCurrentPath->Name);

                    UNICODE_STRING CmpPath = RTL_CONSTANT_STRING(L"\\DEVICE\\HARDDISKVOLUME*\\");
                    BOOLEAN result = FsRtlIsNameInExpression(&CmpPath, &pCurrentPath->Name, TRUE, NULL);
                    FltReleaseFileNameInformation(pCurrentPath);
                    if (!result) {
                        return FLT_POSTOP_FINISHED_PROCESSING;
                    } // ����̹� ����(��Ƽ��) �ֻ��� ���(ex. C:\)�� ���� ��û�� �ƴϸ� return


                    VOID* pCurrentList = NULL;
                    switch (Data->Iopb->Parameters.DirectoryControl.QueryDirectory.FileInformationClass) {
                        case FileDirectoryInformation: {
                            pCurrentList = (PFILE_DIRECTORY_INFORMATION)Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;
                            break;
                        }
                        case FileNamesInformation: {
                            pCurrentList = (PFILE_NAMES_INFORMATION)Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;
                            break;
                        }
                        case FileIdBothDirectoryInformation: {
                            pCurrentList = (PFILE_ID_BOTH_DIR_INFORMATION)Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;
                            break;
                        } // �Ϲ����� UI Ž���⿡�� ���� ������ ��û�� �� ����ϴ� ����ü
                        case FileIdFullDirectoryInformation: {
                            pCurrentList = (PFILE_ID_FULL_DIR_INFORMATION)Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;
                            break;
                        }
                        case FileBothDirectoryInformation: {
                            pCurrentList = (PFILE_BOTH_DIR_INFORMATION)Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;
                            break;
                        } // FindFirstFile() API���� ���� ������ ��û�� �� ����ϴ� ����ü
                        case FileFullDirectoryInformation: {
                            pCurrentList = (PFILE_FULL_DIR_INFORMATION)Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;
                            break;
                        }
                        default: {
                            return FLT_POSTOP_FINISHED_PROCESSING;
                            break;
                        }
                    }
                    Data->IoStatus.Status = STATUS_NO_MORE_ENTRIES;
                    return FLT_POSTOP_FINISHED_PROCESSING;


                    break;
                }
            }
            break;
        }
    }
    return FLT_POSTOP_FINISHED_PROCESSING;
}
// ���� �ڵ�� �Ϻη� �Լ�ȭ ��Ű�� �ʾҽ��ϴ�.
// ���� ��Ź�帳�ϴ�.