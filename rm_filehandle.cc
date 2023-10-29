#include "rm_internal.h"


RM_FileHandle::RM_FileHandle()
{
   // 파일 핸들러가 더티상태인지 체크
   bHdrChanged = FALSE;
   // fileHdr 구조체 내부변수 초기화
   memset(&fileHdr, 0, sizeof(fileHdr));
   // 아무런 페이지도 사용가능하지 않음
   fileHdr.firstFree = RM_PAGE_LIST_END;
}

RM_FileHandle::~RM_FileHandle()
{
}


RC RM_FileHandle::GetRec(const RID &rid, RM_Record &rec) const
{
   RC rc;
   PageNum pageNum;
   SlotNum slotNum;
   PF_PageHandle pageHandle;
   char *pData;

   // rid에서 페이지 번호 추출
   if ((rc = rid.GetPageNum(pageNum)))
      return (rc);

   // rid에서 슬롯 번호 추출
   if ((rc = rid.GetSlotNum(slotNum)))
      return (rc);

   // slotNum 범위 검사
   // PF_FileHandle.GetThisPage()는 pageNum을 처리할 것임
   if (slotNum >= fileHdr.numRecordsPerPage || slotNum < 0)
      return (RM_INVALIDSLOTNUM);
   
   // rid가 가리키는 페이지 가져오기
   if ((rc = pfFileHandle.GetThisPage(pageNum, pageHandle)))
      return (rc);

   // 데이터 가져오기
   if ((rc = pageHandle.GetData(pData)))
      pfFileHandle.UnpinPage(pageNum);

   // rid에 해당하는 레코드가 존재해야 함
   if (!GetBitmap(pData + sizeof(RM_PageHdr), slotNum)) {
      pfFileHandle.UnpinPage(pageNum);
      return (RM_RECORDNOTFOUND);
   }

   // RM_Record에 레코드 복사
   rec.rid = rid;
   if (rec.pData)
      delete [] rec.pData;
   rec.recordSize = fileHdr.recordSize;
   rec.pData = new char[rec.recordSize];
   memcpy(rec.pData,
          pData + fileHdr.pageHeaderSize + slotNum * fileHdr.recordSize, 
          fileHdr.recordSize);

   // 페이지 언핀
   if ((rc = pfFileHandle.UnpinPage(pageNum)))
      return (rc);

   return (0);

err_unpin:
   pfFileHandle.UnpinPage(pageNum);
err_return:
   return (rc);
}


RC RM_FileHandle::InsertRec(const char *pRecordData, RID &rid)
{
   RC rc;
   PageNum pageNum;
   SlotNum slotNum;
   PF_PageHandle pageHandle;
   char *pData;
   RID *pRid;

   // pRecordData는 NULL이면 안됨
   if (pRecordData == NULL)
      return (RM_NULLPOINTER);
 
   // 프리 페이지 목록이 비어 있으면 새 페이지 할당
   if (fileHdr.firstFree == RM_PAGE_LIST_END) {
      // PF_FileHandle::AllocatePage() 호출
      if ((rc = pfFileHandle.AllocatePage(pageHandle)))
         return rc;

      // 페이지 번호 얻기
      if ((rc = pageHandle.GetPageNum(pageNum)))
         return rc;

      // 데이터 포인터 얻기
      if ((rc = pageHandle.GetData(pData)))
         return rc;

      // 페이지 헤더 설정
      ((RM_PageHdr *)pData)->nextFree = RM_PAGE_LIST_END;

      // 다음 포인터를 변경했으므로 페이지를 더티 표시
      if ((rc = pfFileHandle.MarkDirty(pageNum)))
         return rc;

      // 페이지 언핀
      if ((rc = pfFileHandle.UnpinPage(pageNum)))
         return rc;

      // 프리 페이지 목록에 넣기
      fileHdr.firstFree = pageNum;
      bHdrChanged = TRUE;
   }
   // 목록에서 첫 번째 페이지 선택
   else {
      pageNum = fileHdr.firstFree;
   }

   // 새 레코드가 삽입될 페이지 핀
   if ((rc = pfFileHandle.GetThisPage(pageNum, pageHandle)))
      return rc;

   // 데이터 포인터 얻기
   if ((rc = pageHandle.GetData(pData)))
      return rc;

   // 비어 있는 슬롯 찾기
   for (slotNum = 0; slotNum < fileHdr.numRecordsPerPage; slotNum++)
      if (!GetBitmap(pData + sizeof(RM_PageHdr), slotNum))
         break;

   // 프리 슬롯이 있어야 함
   assert(slotNum < fileHdr.numRecordsPerPage);
   
   // rid 할당
   pRid = new RID(pageNum, slotNum);
   rid = *pRid;
   delete pRid;

   // 주어진 레코드 데이터를 버퍼 풀에 복사
   memcpy(pData + fileHdr.pageHeaderSize + slotNum * fileHdr.recordSize, 
          pRecordData, fileHdr.recordSize);

   // 비트 설정
   SetBitmap(pData + sizeof(RM_PageHdr), slotNum);

   // 비어 있는 슬롯 찾기
   for (slotNum = 0; slotNum < fileHdr.numRecordsPerPage; slotNum++)
      if (!GetBitmap(pData + sizeof(RM_PageHdr), slotNum))
         break;

   // 필요한 경우 프리 페이지 목록에서 페이지 제거
   if (slotNum == fileHdr.numRecordsPerPage) {
      fileHdr.firstFree = ((RM_PageHdr *)pData)->nextFree;
      bHdrChanged = TRUE;
      ((RM_PageHdr *)pData)->nextFree = RM_PAGE_FULL;
   }

   // 비트맵을 변경했기 때문에 헤더 페이지를 더티 표시
   if ((rc = pfFileHandle.MarkDirty(pageNum)))
      return rc;

   // 페이지 언핀
   if ((rc = pfFileHandle.UnpinPage(pageNum)))
      return rc;

   // 성공 반환
   return (0);
}

RC RM_FileHandle::DeleteRec(const RID &rid)
{
   RC rc;
   PageNum pageNum;
   SlotNum slotNum;
   PF_PageHandle pageHandle;
   char *pData;

   // rid에서 페이지 번호 추출
   if (rc = rid.GetPageNum(pageNum))
      return rc;

   // rid에서 슬롯 번호 추출
   if (rc = rid.GetSlotNum(slotNum))
      return rc;

   // 슬롯Num 경계 확인
   if (slotNum >= fileHdr.numRecordsPerPage || slotNum < 0)
      return (RM_INVALIDSLOTNUM);
   
   if (rc = pfFileHandle.GetThisPage(pageNum, pageHandle))
      return rc;

   if (rc = pageHandle.GetData(pData)) {
      pfFileHandle.UnpinPage(pageNum);
      return rc;
   }

   if (!GetBitmap(pData + sizeof(RM_PageHdr), slotNum)) { 
      // 해당 rid에 해당하는 레코드가 존재해야 함
      pfFileHandle.UnpinPage(pageNum);
      return (RM_RECORDNOTFOUND);
   }
   // 비트 지우기
   ClrBitmap(pData + sizeof(RM_PageHdr), slotNum);
   
   memset(pData + fileHdr.pageHeaderSize + slotNum * fileHdr.recordSize, 
          '\0', fileHdr.recordSize);
   // 빈 슬롯 찾기
   for (slotNum = 0; slotNum < fileHdr.numRecordsPerPage; slotNum++)
      if (GetBitmap(pData + sizeof(RM_PageHdr), slotNum))
         break;

   // 페이지가 비어 있으면 (삭제된 레코드가 마지막이었으면) 페이지를 삭제한다.
   // 이렇게 하면 차지하고 있는 페이지의 총 수를 최대한 작게 유지할 수 있다.
   if (slotNum == fileHdr.numRecordsPerPage) { 
      fileHdr.firstFree = ((RM_PageHdr *)pData)->nextFree;
      bHdrChanged = TRUE;
      if (rc = pfFileHandle.MarkDirty(pageNum)) 
         return rc;
      if (rc = pfFileHandle.UnpinPage(pageNum))
         return rc;
      return pfFileHandle.DisposePage(pageNum);
   }

   if (((RM_PageHdr *)pData)->nextFree == RM_PAGE_FULL) { 
      // 페이지를 프리 페이지 목록에 추가
      ((RM_PageHdr *)pData)->nextFree = fileHdr.firstFree;
      fileHdr.firstFree = pageNum;
      bHdrChanged = TRUE;
   }

   // 비트맵이 변경되었으므로 헤더 페이지를 더티 상태로 표시
   if (rc = pfFileHandle.MarkDirty(pageNum)) {
      pfFileHandle.UnpinPage(pageNum);
      return rc;
   }

   rc = pfFileHandle.UnpinPage(pageNum);
   if (rc) return rc;
 
   return (0);
}


RC RM_FileHandle::UpdateRec(const RM_Record &rec)
{
   RC rc;
   RID rid;
   PageNum pageNum;
   SlotNum slotNum;
   PF_PageHandle pageHandle;
   char *pData;
   char *pRecordData;

   // RID 얻기
   if ((rc = rec.GetRid(rid)))
   {
      return rc;
   }

   // 레코드 데이터 얻기
   if ((rc = rec.GetData(pRecordData)))
   {
      return rc;
   }

   // rid에서 페이지 번호 추출
   if ((rc = rid.GetPageNum(pageNum)))
   {
      return rc;
   }

   // rid에서 슬롯 번호 추출
   if ((rc = rid.GetSlotNum(slotNum)))
   {
      return rc;
   }

   // 슬롯Num 경계 검사
   if (slotNum >= fileHdr.numRecordsPerPage || slotNum < 0)
   {
      return RM_INVALIDSLOTNUM;
   }

   // 업데이트하는 레코드와 파일 핸들의 recordSize가 일치해야 함
   if (rec.recordSize != fileHdr.recordSize)
   {
      return RM_INVALIDRECSIZE;
   }

   // rid가 가리키는 페이지 얻기
   if ((rc = pfFileHandle.GetThisPage(pageNum, pageHandle)))
   {
      return rc;
   }

   // 데이터 얻기
   if ((rc = pageHandle.GetData(pData)))
   {
      pfFileHandle.UnpinPage(pageNum);
      return rc;
   }

   // rid에 해당하는 레코드가 존재해야 함
   if (!GetBitmap(pData + sizeof(RM_PageHdr), slotNum))
   {
      pfFileHandle.UnpinPage(pageNum);
      return RM_RECORDNOTFOUND;
   }

   // 주어진 레코드의 데이터(pRecordData)를 페이지 내의 적절한 위치(pData + fileHdr.pageHeaderSize + slotNum * fileHdr.recordSize)로 복사
   // 페이지 시작 위치+페이지 헤더 크기 + slotNum*레코드 크기의 위치에 precorddata로 복사
   memcpy(pData + fileHdr.pageHeaderSize + slotNum * fileHdr.recordSize, pRecordData, fileHdr.recordSize);

   // 헤더 페이지를 더티 상태로 표시
   if ((rc = pfFileHandle.MarkDirty(pageNum)))
   {
      pfFileHandle.UnpinPage(pageNum);
      return rc;
   }

   // 페이지 언핀
   if ((rc = pfFileHandle.UnpinPage(pageNum)))
   {
      return rc;
   }
 
   return OK_RC;
}

RC RM_FileHandle::ForcePages(PageNum pageNum)
{
    RC rc;

    // 파일 헤더에 변경이 발생한 경우, 파일이 열릴 때 헤더에 변경 사항을 작성
    if (bHdrChanged) 
    {
        PF_PageHandle pageHandle;
        char* pData;

        // 헤더 페이지를 가져온다.
        if (rc = pfFileHandle.GetFirstPage(pageHandle))
            return rc; 

        // 헤더 정보를 작성할 포인터를 얻는다.
        if (rc = pageHandle.GetData(pData))
        {
            pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
            return rc;
        }

        // 파일 헤더를 버퍼 풀에 쓴다.
        memcpy(pData, &fileHdr, sizeof(fileHdr));

        // 헤더 페이지를 변경된 것으로 표시한다.
        if (rc = pfFileHandle.MarkDirty(RM_HEADER_PAGE_NUM))
        {
            pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
            return rc;
        }

        // 헤더 페이지의 고정을 해제한다.
        if (rc = pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM))
            return rc;

        // 헤더 페이지를 강제로 디스크에 기록한다.
        if (rc = pfFileHandle.ForcePages(RM_HEADER_PAGE_NUM))
            return rc;

        // 파일 헤더가 변경되지 않은 상태로 되돌린다.
        bHdrChanged = FALSE;
    }

    // 주어진 페이지 번호에 해당하는 페이지를 강제로 디스크에 기록한다.
    if (rc = pfFileHandle.ForcePages(pageNum))
        return rc;

    return 0;
}


// 바이트 배열: [01010101, 10011001] 
// char 타입을 사용하기 때문에 8비트로 표현할 것
int RM_FileHandle::GetBitmap(char *map, int idx) const
{
   return (map[idx / 8] & (1 << (idx % 8))) != 0;
}

// 원하는 위치의 비트만 1로 바꾸고 or 연산
void RM_FileHandle::SetBitmap(char *map, int idx) const
{
   map[idx / 8] |= (1 << (idx % 8));
}


void RM_FileHandle::ClrBitmap(char *map, int idx) const
{
   map[idx / 8] &= ~(1 << (idx % 8));
}

