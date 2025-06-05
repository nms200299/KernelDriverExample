/*++

Module Name:

    FsFilter3.c

Abstract:

    미니필터를 이용한 폴더/파일 숨김 기능 구현

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
      // ▲ 디렉터리 제어 IRP
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
    // 문자열 비교를 위해 UNICODE_STRING 구조체를 채워줌

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
    } // IRQL 검사 (PASSIVE_LEVEL이 아닌 상태에서 Paged-Pool 메모리에 접근하지 못합니다)

    NTSTATUS status;
    switch (Data->Iopb->MajorFunction) {
        case IRP_MJ_DIRECTORY_CONTROL: {
        // 디렉터리 제어에 관한 IRP이고,
            switch (Data->Iopb->MinorFunction) {
                case IRP_MN_QUERY_DIRECTORY: {
                    // 디렉터리 내부 파일 질의에 관한 IRP일 때,
                    if (!NT_SUCCESS(Data->IoStatus.Status)) {
                        return FLT_POSTOP_FINISHED_PROCESSING;
                    } // 요청에 대한 응답이 성공이 아니면 return

                    if (Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer == NULL){
                        return FLT_POSTOP_FINISHED_PROCESSING;
                    } // 반환된 파일 정보가 없으면 return

                    PFLT_FILE_NAME_INFORMATION pCurrentPath = NULL;
                    status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED, &pCurrentPath);
                    if (!NT_SUCCESS(status)) {
                        return FLT_POSTOP_FINISHED_PROCESSING;
                    } // 요청 경로에 대한 정보를 얻어오지 못하면 return

                   

                    UNICODE_STRING CmpPath = RTL_CONSTANT_STRING(L"\\DEVICE\\HARDDISKVOLUME*\\TEST");
                    BOOLEAN result = FsRtlIsNameInExpression(&CmpPath, &pCurrentPath->Name, TRUE, NULL);

                    FltReleaseFileNameInformation(pCurrentPath);
                    if (!result) {
                        return FLT_POSTOP_FINISHED_PROCESSING;
                    } // 드라이버 볼륨(파티션) 최상위 경로(ex. C:\)에 대한 요청이 아니면 return

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
                        } // 일반적인 UI 탐색기에서 파일 정보를 요청할 때 사용하는 구조체
                        //case FileIdFullDirectoryInformation: {
                        //    PFILE_ID_FULL_DIR_INFORMATION pCurrentList = (PFILE_ID_FULL_DIR_INFORMATION)Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;
                        //    break;
                        //}
                        //case FileBothDirectoryInformation: {
                        //    PFILE_BOTH_DIR_INFORMATION pCurrentList = (PFILE_BOTH_DIR_INFORMATION)Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;
                        //    break;
                        //} // FindFirstFile() API에서 파일 정보를 요청할 때 사용하는 구조체
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
// 예제 코드의 분기문으로 일부러 함수화 시키지 않았습니다.
// 양해 부탁드립니다.

