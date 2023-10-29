#include "rm_internal.h"


RM_Manager::RM_Manager(PF_Manager &pfm)
{
   pPfm = &pfm;
}

RM_Manager::~RM_Manager()
{
   pPfm = NULL;
}


RC RM_Manager::CreateFile(const char *fileName, unsigned recordSize)
{
   RC rc;
   PF_FileHandle pfFileHandle;
   PF_PageHandle pageHandle;
   char* pData;
   RM_FileHdr *fileHdr;

   // 검증: recordSize가 너무 크거나 작지 않아야 함
   // PF_Manager::CreateFile()은 fileName을 처리할 것임
   if (recordSize >= PF_PAGE_SIZE - sizeof(RM_PageHdr) || recordSize < 1)
      // 테스트: 잘못된 recordSize
      return (RM_INVALIDRECSIZE);

   // PF_Manager::CreateFile() 호출
   if ((rc = pPfm->CreateFile(fileName)))
   {
      pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
      return(rc);
   }

   // PF_Manager::OpenFile() 호출
   if ((rc = pPfm->OpenFile(fileName, pfFileHandle)))
   {
      pPfm->DestroyFile(fileName);
      return(rc);
   }
   // 헤더 페이지 할당 (pageNum은 0이어야 함)
   if ((rc = pfFileHandle.AllocatePage(pageHandle))) {
   {
      pPfm->CloseFile(pfFileHandle);
      return(rc);
   }
   // 헤더 정보를 작성할 포인터 얻기
   if ((rc = pageHandle.GetData(pData)))
   {
      pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
      return(rc);
   }

   // 파일 헤더 작성 (버퍼 풀에)
   fileHdr = (RM_FileHdr *)pData;
   fileHdr->firstFree = RM_PAGE_LIST_END;
   fileHdr->recordSize = recordSize;
   // 페이지당 레코드 수 -> PGSIZE - RM_PageHdr - 1 / recordsize + 1/8
   fileHdr->numRecordsPerPage = (PF_PAGE_SIZE - sizeof(RM_PageHdr) - 1) 
                                / (recordSize + 1.0/8);
   if (recordSize * (fileHdr->numRecordsPerPage + 1) 
      + fileHdr->numRecordsPerPage / 8 
      <= PF_PAGE_SIZE - sizeof(RM_PageHdr) - 1)
      fileHdr->numRecordsPerPage++;
   fileHdr->pageHeaderSize = sizeof(RM_PageHdr) + (fileHdr->numRecordsPerPage + 7) / 8;
   fileHdr->numRecords = 0;

   // 헤더 페이지를 더티 상태로 표시
   if ((rc = pfFileHandle.MarkDirty(RM_HEADER_PAGE_NUM)))
   {
      pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
      return(rc);
   }

   
   // 헤더 페이지 언핀
   if ((rc = pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM)))
   {
      pPfm->CloseFile(pfFileHandle);
      return(rc);
   }
   
   // PF_Manager::CloseFile() 호출
   if ((rc = pPfm->CloseFile(pfFileHandle)))
   {
      pPfm->DestroyFile(fileName);
      return(rc);
   }

   // 정상 반환
   return (0);

RC RM_Manager::DestroyFile(const char *fileName)
{
   RC rc;
   if ((rc = pPfm->DestroyFile(fileName)))
      return (rc);
   return OK_RC;
}

RC RM_Manager::OpenFile(const char *fileName, RM_FileHandle &fileHandle)
{
   RC rc;
   PF_PageHandle pageHandle;
   char* pData;

   // PF_Manager::OpenFile() 호출
   rc = pPfm->OpenFile(fileName, fileHandle.pfFileHandle);
   if (rc) {
      return rc;
   }

   // 헤더 페이지 가져오기
   rc = fileHandle.pfFileHandle.GetFirstPage(pageHandle);
   if (rc) {
      pPfm->CloseFile(fileHandle.pfFileHandle);
      return rc;
   }

   // 헤더 정보가 있는 포인터 얻기
   rc = pageHandle.GetData(pData);
   if (rc) {
      fileHandle.pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
      pPfm->CloseFile(fileHandle.pfFileHandle);
      return rc;
   }

   // 파일 헤더 읽기 (버퍼 풀에서 RM_FileHandle로)
   memcpy(&fileHandle.fileHdr, pData, sizeof(fileHandle.fileHdr));

   // 헤더 페이지 언핀
   rc = fileHandle.pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
   if (rc) {
      pPfm->CloseFile(fileHandle.pfFileHandle);
      return rc;
   }
   // 파일 헤더를 변경되지 않은 상태로 설정
   fileHandle.bHdrChanged = FALSE;
   return (0);
}


RC RM_Manager::CloseFile(RM_FileHandle &fileHandle)
{
   RC rc;
   
   // 파일이 열려 있는 동안 헤더에 변경사항이 있었다면 파일 헤더를 다시 작성
   if (fileHandle.bHdrChanged) {
      PF_PageHandle pageHandle;
      char* pData;

      // 헤더 페이지 가져오기
      rc = fileHandle.pfFileHandle.GetFirstPage(pageHandle);
      if (rc) {
         return rc;
      }

      // 헤더 정보가 작성될 포인터 얻기
      rc = pageHandle.GetData(pData);
      if (rc) {
         fileHandle.pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
         return rc;
      }

      // 파일 헤더 작성 (버퍼 풀로)
      memcpy(pData, &fileHandle.fileHdr, sizeof(fileHandle.fileHdr));

      // 헤더 페이지를 더티로 표시
      rc = fileHandle.pfFileHandle.MarkDirty(RM_HEADER_PAGE_NUM);
      if (rc) {
         fileHandle.pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
         return rc;
      }

      // 헤더 페이지 언핀
      rc = fileHandle.pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
      if (rc) {
         return rc;
      }

      // 파일 헤더 변경되지 않은 상태로 설정
      fileHandle.bHdrChanged = FALSE;
   }

   // PF_Manager::CloseFile() 호출
   rc = pPfm->CloseFile(fileHandle.pfFileHandle);
   if (rc) {
      return rc;
   }

   // 멤버 변수 재설정
   memset(&fileHandle.fileHdr, 0, sizeof(fileHandle.fileHdr));
   fileHandle.fileHdr.firstFree = RM_PAGE_LIST_END;

   // 정상 반환
   return OK_RC;
}
