/*++

Module Name:

    FsFilter3.c

Abstract:

    �̴����͸� �̿��� ���� ���� �� ���� ����͸� ����

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
FsFilter3Unload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
FsFilter3PreOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    );


FLT_POSTOP_CALLBACK_STATUS
FsFilter3PostOperation (
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
#pragma alloc_text(PAGE, FsFilter3Unload)
#endif

//
//  operation registration
//

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
    { IRP_MJ_CREATE,
      0,
      FsFilter3PreOperation,
      FsFilter3PostOperation },
      // �� ���� �ڵ� ����
    { IRP_MJ_READ,
      0,
      FsFilter3PreOperation,
      FsFilter3PostOperation },
      // �� ���� ������ �б�
    { IRP_MJ_WRITE,
      0,
      FsFilter3PreOperation,
      FsFilter3PostOperation },
      // �� ���� ������ ����
    { IRP_MJ_SET_INFORMATION ,
      0,
      FsFilter3PreOperation,
      FsFilter3PostOperation },
      // �� ���� �̸� ����, �̵�, ����
    { IRP_MJ_CLOSE,
      0,
      FsFilter3PreOperation,
      FsFilter3PostOperation },
      // �� ���� �ڵ� �ݱ� 
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
    FsFilter3Unload,                    //  MiniFilterUnload 
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

    status = STATUS_SUCCESS;

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
FsFilter3Unload (
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
FLT_PREOP_CALLBACK_STATUS
FsFilter3PreOperation (
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
    PFLT_FILE_NAME_INFORMATION pFileInfo = NULL;
    UNICODE_STRING DstFilePath;
    PUNICODE_STRING pOrgFilePath = NULL;
    
    switch (Data->Iopb->MajorFunction) {
        case IRP_MJ_SET_INFORMATION: {
            switch (Data->Iopb->Parameters.SetFileInformation.FileInformationClass) {
                case FileRenameInformation:
                case FileRenameInformationEx:
                case FileRenameInformationBypassAccessCheck:
                case FileRenameInformationExBypassAccessCheck:{
                    // ���� ����, �̵��� ���, ������ ��ο� ��õ� ���� �̸��� ����� �� �־� �Ʒ� �������� ó���մϴ�.  
                    PFILE_RENAME_INFORMATION pRenameInfo =
                        (PFILE_RENAME_INFORMATION)Data->Iopb->Parameters.SetFileInformation.InfoBuffer;

                    if ((pRenameInfo == NULL) || (pRenameInfo->FileNameLength == 0)) {
                        return FLT_PREOP_SUCCESS_NO_CALLBACK;
                        // ������ ��θ� �� �� ���� ���, �ش� IRP�� ����մϴ�. (PostOperation �̼���)
                    }

                    DstFilePath.Length = (USHORT)pRenameInfo->FileNameLength;
                    DstFilePath.MaximumLength = DstFilePath.Length;
                    DstFilePath.Buffer = pRenameInfo->FileName;
                    pOrgFilePath = &DstFilePath;
                    // IRP�� ������ ��θ� �� ������� ����ϴ�.
                    break;
                }
            }
        }


        default: {
            status = FltGetFileNameInformation(Data, FLT_FILE_NAME_OPENED, &pFileInfo);
            // IRP�� ��õ� ���� ��θ� �˾Ƴ��ϴ�

            if ((!NT_SUCCESS(status)) || (pFileInfo == NULL)) {
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            } // ���� ��θ� �� �� ������ �ش� IRP�� ����մϴ�. (PostOperation �̼���)

            if (Data->Iopb->MajorFunction == IRP_MJ_CLOSE) {
                status = FltParseFileNameInformation(pFileInfo);
                if (!NT_SUCCESS(status)) {
                    FltReleaseFileNameInformation(pFileInfo);
                    return FLT_POSTOP_FINISHED_PROCESSING;
                }
                // ���� ��θ� �Ľ��Ͽ� ����ü ����� ä���ݴϴ�. (���� ��, Ȯ����, ...)
                ULONG AccessPID = FltGetRequestorProcessId(Data);
                // �����ϴ� PID�� ���մϴ�.
                DbgPrint("[MINIFILTER] File Request : PID(%lu)\tIRP(CLOSE)\tPath(%wZ)\n", (ULONG)(ULONG_PTR)AccessPID, &pFileInfo->FinalComponent);
            } // ���� �ڵ� �ݱ� ��û (CloseHandle())

            pOrgFilePath = &pFileInfo->Name;
            break;
        }
    }

    UNICODE_STRING CmpFilePath = RTL_CONSTANT_STRING(L"*\\TEST.TXT");
    BOOLEAN result = FsRtlIsNameInExpression(&CmpFilePath, pOrgFilePath, TRUE, NULL);
    // ��ҹ��� ���о��� ���� ��ο� test.txt�� ��Ī�Ǵ��� Ȯ���մϴ�.
    if (pFileInfo != NULL) {
        FltReleaseFileNameInformation(pFileInfo);
        pFileInfo = NULL;
    } // FltGetFileNameInformation �Լ��� �Ҵ���� ���� ��ΰ� �ִ� ���, �����մϴ�. 

    if (result) {
        DbgPrint("[MINIFILTER] test.txt Denied.\n");
        Data->IoStatus.Status = STATUS_ACCESS_DENIED;
        Data->IoStatus.Information = 0;
        return FLT_PREOP_COMPLETE;
        // test.txt�� ��Ī�ȴٸ� IRP�� �ź��մϴ�.
    }

    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
    // �� ���ǿ� �ش����� ������ IRP�� ����մϴ�. (PostOperation ����)
}


FLT_POSTOP_CALLBACK_STATUS
FsFilter3PostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    )
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );
    UNREFERENCED_PARAMETER( Flags );
    if (KeGetCurrentIrql() > PASSIVE_LEVEL) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    } // IRQL �˻� (PASSIVE_LEVEL�� �ƴ� ���¿��� Paged-Pool �޸𸮿� �������� ���մϴ�)


    NTSTATUS status;
    PFLT_FILE_NAME_INFORMATION pFileInfo = NULL;

    status = FltGetFileNameInformation(Data, FLT_FILE_NAME_OPENED, &pFileInfo);
    if (!NT_SUCCESS(status)) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }
    // IRP�� ��õ� ���� ��θ� �˾Ƴ��ϴ�


    status = FltParseFileNameInformation(pFileInfo);
    if (!NT_SUCCESS(status)) {
        FltReleaseFileNameInformation(pFileInfo);
        return FLT_POSTOP_FINISHED_PROCESSING;
    }
    // ���� ��θ� �Ľ��Ͽ� ����ü ����� ä���ݴϴ�. (���� ��, Ȯ����, ...)

    ULONG AccessPID = FltGetRequestorProcessId(Data);
    // �����ϴ� PID�� ���մϴ�.

    switch (Data->Iopb->MajorFunction) {
        case IRP_MJ_CREATE: {
            DbgPrint("[MINIFILTER] File Request : PID(%lu)\tIRP(CREATE)\tPath(%wZ)\n", (ULONG)(ULONG_PTR)AccessPID, &pFileInfo->FinalComponent);
            break;
        } // ���� �ڵ� ���� ��û (CreateFile())

        case IRP_MJ_READ: {
            DbgPrint("[MINIFILTER] File Request : PID(%lu)\tIRP(READ)\tPath(%wZ)\n", (ULONG)(ULONG_PTR)AccessPID, &pFileInfo->FinalComponent);
            break;
        } // ���� �б� ��û (ReadFile())

        case IRP_MJ_WRITE: {
            DbgPrint("[MINIFILTER] File Request : PID(%lu)\tIRP(WRITE)\tPath(%wZ)\n", (ULONG)(ULONG_PTR)AccessPID, &pFileInfo->FinalComponent);
            break;
        } // ���� ���� ��û (WriteFile())

        case IRP_MJ_SET_INFORMATION: {
            FILE_INFORMATION_CLASS FileInfoClass = Data->Iopb->Parameters.SetFileInformation.FileInformationClass;
            PVOID FileInfoBuffer = Data->Iopb->Parameters.SetFileInformation.InfoBuffer;

            switch (FileInfoClass) {
                case FileDispositionInformation: {
                    PFILE_DISPOSITION_INFORMATION DispositionInfo = (PFILE_DISPOSITION_INFORMATION)FileInfoBuffer;
                    if (DispositionInfo->DeleteFile) {
                        DbgPrint("[MINIFILTER] File Request : PID(%lu)\tIRP(DELETE)\tPath(%wZ)\n", (ULONG)(ULONG_PTR)AccessPID, &pFileInfo->FinalComponent);
                    }
                    break;
                } // ���� ���� ��û (DeleteFile() ��)

                case FileDispositionInformationEx: {
                    PFILE_DISPOSITION_INFORMATION_EX DispositionInfoEx = (PFILE_DISPOSITION_INFORMATION_EX)FileInfoBuffer;
                    if (DispositionInfoEx->Flags != FILE_DISPOSITION_DO_NOT_DELETE) {
                        DbgPrint("[MINIFILTER] File Request : PID(%lu)\tIRP(DELETE_EX)\tPath(%wZ)\n", (ULONG)(ULONG_PTR)AccessPID, &pFileInfo->FinalComponent);
                    }
                } // ���� ���� ��û (NtDeleteFile() ��) 

                case FileRenameInformation:
                case FileRenameInformationBypassAccessCheck:
                case FileRenameInformationEx:
                case FileRenameInformationExBypassAccessCheck: {
                    DbgPrint("[MINIFILTER] File Request : PID(%lu)\tIRP(MOVE)\tPath(%wZ)\n", (ULONG)(ULONG_PTR)AccessPID, &pFileInfo->FinalComponent);
                } // ���� �̵�, �̸� ���� ��û (MoveFile() ��) 
            }
        }
    }

    FltReleaseFileNameInformation(pFileInfo);
    return FLT_POSTOP_FINISHED_PROCESSING;
}
