
#include "rm_internal.h"

RM_FileScan::RM_FileScan()
{
   // 시작 변수 초기화
   bScanOpen = FALSE;
   curPageNum = RM_HEADER_PAGE_NUM;
   curSlotNum = 0;

   pFileHandle = NULL;
   attrType = INT;
   attrLength = sizeof(int);
   attrOffset = 0;
   compOp = NO_OP;
   value = NULL;
   pinHint = NO_HINT;
}


RM_FileScan::~RM_FileScan()         
{
}

RC RM_FileScan::OpenScan(const RM_FileHandle &fileHandle, 
                         AttrType   _attrType,
                         int        _attrLength,
                         int        _attrOffset,
                         CompOp     _compOp,
                         void       *_value,
                         ClientHint _pinHint)
{
   // 아직 ScanOpen이 닫혀있어야함
   if (bScanOpen)
      return (RM_SCANOPEN);

   // 파일 핸들러가 있어야 함
   if (fileHandle.fileHdr.recordSize == 0)
      return (RM_CLOSEDFILE);

   //어떤 동작을 할건지
   switch (_compOp) {
   case EQ_OP:
   case LT_OP:
   case GT_OP:
   case LE_OP:
   case GE_OP:
   case NE_OP:
   case NO_OP:
      break;

   default:
      return (RM_INVALIDCOMPOP);
   }
   
   if (_compOp != NO_OP) {
      if (_value == NULL)
         return (RM_NULLPOINTER);

      // attrType, attrLength에 대한 확인
      switch (_attrType) {
      case INT:
      case FLOAT:
         if (_attrLength != 4)
            return (RM_INVALIDATTR);
         break;

      case STRING:
         if (_attrLength < 1 || _attrLength > MAXSTRINGLEN)
            return (RM_INVALIDATTR);
         break;

      default:
         return (RM_INVALIDATTR);
      }

      // attrOffset 체크
      if (_attrOffset < 0 
          || _attrOffset + _attrLength > fileHandle.fileHdr.recordSize)
         return (RM_INVALIDATTR);
   }

   // 로컬 변수에 매개변수 복사하기
   pFileHandle = (RM_FileHandle *)&fileHandle;
   attrType    = _attrType;
   attrLength  = _attrLength;
   attrOffset  = _attrOffset;
   compOp      = _compOp;
   value       =  _value;
   pinHint     = _pinHint;

   // 로컬 상태 변수 설정하기
   bScanOpen = TRUE;
   curPageNum = RM_HEADER_PAGE_NUM;
   curSlotNum = pFileHandle->fileHdr.numRecordsPerPage;

   // Return ok
   return (0);
}


RC RM_FileScan::GetNextRec(RM_Record &rec)
{
    RC rc;
    PF_PageHandle pageHandle;
    char *pData;
    RID *curRid;

    // 스캔오픈 확인
    if (!bScanOpen)
        return (RM_CLOSEDSCAN);

    // 파일 핸들러 확인
    if (pFileHandle->fileHdr.recordSize == 0)
        return (RM_CLOSEDFILE);

    bool needRepeat = false;
    do {
        needRepeat = false;
        // 필요한 경우 다른 페이지를 가져옴
        if (curSlotNum == pFileHandle->fileHdr.numRecordsPerPage) {
            if ((rc = pFileHandle->pfFileHandle.GetNextPage(curPageNum, pageHandle)))
                return rc;
            if ((rc = pageHandle.GetPageNum(curPageNum)))
                return rc;
            curSlotNum = 0;
        }
        // 이전 GetNextRec() 호출에서 전체 페이지를 처리하지 않았다면
        else {
            rc = pFileHandle->pfFileHandle.GetThisPage(curPageNum, pageHandle);
            if (rc == PF_INVALIDPAGE) {
                needRepeat = true;
                continue;
            } else if (rc) {
                return rc;
            }
        }

        if ((rc = pageHandle.GetData(pData)))
            return rc;

        // 스캔 조건을 기반으로 다음 레코드를 찾음
        FindNextRecInCurPage(pData);

        // 이 페이지에 일치하는 항목이 없다면 다음 페이지로 이동
        if (curSlotNum == pFileHandle->fileHdr.numRecordsPerPage) {
            if ((rc = pFileHandle->pfFileHandle.UnpinPage(curPageNum)))
                return rc;
            
            needRepeat = true;
        }

    } while (needRepeat);

    // 일치: 주어진 위치에 레코드 복사
    curRid = new RID(curPageNum, curSlotNum);
    rec.rid = *curRid;
    delete curRid;

    if (rec.pData)
        delete[] rec.pData;
    rec.recordSize = pFileHandle->fileHdr.recordSize;
    rec.pData = new char[rec.recordSize];
    memcpy(rec.pData,
           pData + pFileHandle->fileHdr.pageHeaderSize + curSlotNum * pFileHandle->fileHdr.recordSize,
           pFileHandle->fileHdr.recordSize);

    curSlotNum++;

    // 이 페이지에 더 이상 일치하는 레코드가 없다면, 다음 GetNextRec() 호출에서 이 페이지에 액세스할 필요가 없음
    FindNextRecInCurPage(pData);

    if ((rc = pFileHandle->pfFileHandle.UnpinPage(curPageNum)))
        return rc;

    return (0);
}


void RM_FileScan::FindNextRecInCurPage(char *pData)
{
    for ( ; curSlotNum < pFileHandle->fileHdr.numRecordsPerPage; curSlotNum++) {
        float cmp;
        int i;
        float f;

        // 빈 슬롯은 건너뛴다
        if (!pFileHandle->GetBitmap(pData + sizeof(RM_PageHdr), curSlotNum))
            continue;
        
        // compOp이 NO_OP이면 바로 종료
        if (compOp == NO_OP)
            break;

        // 속성 유형에 따라 비교를 수행한다
        switch (attrType) {
        case INT:
            memcpy(&i,
                   pData + pFileHandle->fileHdr.pageHeaderSize
                   + curSlotNum * pFileHandle->fileHdr.recordSize 
                   + attrOffset,
                   sizeof(int));
            cmp = i - *((int *)value);
            break;

        case FLOAT:
            memcpy(&f,
                   pData + pFileHandle->fileHdr.pageHeaderSize
                   + curSlotNum * pFileHandle->fileHdr.recordSize 
                   + attrOffset,
                   sizeof(float));
            cmp = f - *((float *)value);
            break;

        case STRING:
            cmp = memcmp(pData + pFileHandle->fileHdr.pageHeaderSize
                         + curSlotNum * pFileHandle->fileHdr.recordSize 
                         + attrOffset,
                         value,
                         attrLength);
            break;
        }

        // 비교 연산자에 따라 결정을 내린다
        if ((compOp == EQ_OP && cmp == 0)
            || (compOp == LT_OP && cmp < 0)
            || (compOp == GT_OP && cmp > 0)
            || (compOp == LE_OP && cmp <= 0)
            || (compOp == GE_OP && cmp >= 0)
            || (compOp == NE_OP && cmp != 0))
            break;
    }
}

RC RM_FileScan::CloseScan()
{
    // 검증: 'this'는 반드시 열려 있어야 함
    if (!bScanOpen)
        return (RM_CLOSEDSCAN);

    // 멤버 변수 초기화
    bScanOpen = FALSE;
    curPageNum = RM_HEADER_PAGE_NUM;
    curSlotNum = 0;
    pFileHandle = NULL;
    attrType = INT;
    attrLength = sizeof(int);
    attrOffset = 0;
    compOp = NO_OP;
    value = NULL;
    pinHint = NO_HINT;

    // 정상적으로 반환
    return (0);
}