# KernelDriverExample
 커널 드라이버 예제코드

|분류|항목|내용|소스 파일|실행 파일|
|---|----|----------------|---|---|
|파일 시스템|[파일 접근 차단 구현](https://github.com/nms200299/KernelDriverExample/tree/main/FileSystem_Monitoring%26Filtering)|미니필터를 이용해 파일 시스템을 모니터링하고 특정 파일(*\test.txt)의 접근을 차단합니다.|[링크 (.c)](https://github.com/nms200299/KernelDriverExample/blob/main/FileSystem_Monitoring%26Filtering/src/FsFilter3.c)|[링크 (.zip)](https://github.com/nms200299/KernelDriverExample/blob/main/FileSystem_Monitoring%26Filtering/bin/x64.zip)|
||[MBR 보호 기능 구현](https://github.com/nms200299/KernelDriverExample/tree/main/FileSystem_MBR_Protect)|미니필터를 이용해 MBR 영역을 보호하고, 접근하는 프로세스를 강제 종료 시킵니다.|[링크 (.c)](https://github.com/nms200299/KernelDriverExample/blob/main/FileSystem_MBR_Protect/src/FsFilter3.c)|[링크 (.zip)](https://github.com/nms200299/KernelDriverExample/blob/main/FileSystem_MBR_Protect/bin/x64.zip)|
||[파일 및 폴더 숨김 기능 구현](https://github.com/nms200299/KernelDriverExample/tree/main/FileSystem_FileHide)|미니필터를 이용해 특정 파일 및 폴더를 숨길 수 있습니다.|[링크 (.c)](https://github.com/nms200299/KernelDriverExample/blob/main/FileSystem_FileHide/src/FsFilter3.c)|[링크 (.zip)](https://github.com/nms200299/KernelDriverExample/blob/main/FileSystem_FileHide/bin/x64.zip)|
||[Context를 활용한 파일 시스템 모니터링 구현](https://github.com/nms200299/KernelDriverExample/tree/main/FileSystem_FileHide)|미니필터의 Context와 이중 원형리스트를 이용하여 IRP 연속성 있는 파일 시스템 모니터링을 할 수 있습니다.|[링크 (.c)](https://github.com/nms200299/KernelDriverExample/blob/main/FileSystem_Context_Monitoring/src/FsFilter3.c)|[링크 (.zip)](https://github.com/nms200299/KernelDriverExample/blob/main/FileSystem_Context_Monitoring/bin/x64.zip)|

