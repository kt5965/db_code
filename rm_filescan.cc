
#include "rm_internal.h"

RM_FileScan::RM_FileScan()
{
   // ���� ���� �ʱ�ȭ
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
   // ���� ScanOpen�� �����־����
   if (bScanOpen)
      return (RM_SCANOPEN);

   // ���� �ڵ鷯�� �־�� ��
   if (fileHandle.fileHdr.recordSize == 0)
      return (RM_CLOSEDFILE);

   //� ������ �Ұ���
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

      // attrType, attrLength�� ���� Ȯ��
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

      // attrOffset üũ
      if (_attrOffset < 0 
          || _attrOffset + _attrLength > fileHandle.fileHdr.recordSize)
         return (RM_INVALIDATTR);
   }

   // ���� ������ �Ű����� �����ϱ�
   pFileHandle = (RM_FileHandle *)&fileHandle;
   attrType    = _attrType;
   attrLength  = _attrLength;
   attrOffset  = _attrOffset;
   compOp      = _compOp;
   value       =  _value;
   pinHint     = _pinHint;

   // ���� ���� ���� �����ϱ�
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

    // ��ĵ���� Ȯ��
    if (!bScanOpen)
        return (RM_CLOSEDSCAN);

    // ���� �ڵ鷯 Ȯ��
    if (pFileHandle->fileHdr.recordSize == 0)
        return (RM_CLOSEDFILE);

    bool needRepeat = false;
    do {
        needRepeat = false;
        // �ʿ��� ��� �ٸ� �������� ������
        if (curSlotNum == pFileHandle->fileHdr.numRecordsPerPage) {
            if ((rc = pFileHandle->pfFileHandle.GetNextPage(curPageNum, pageHandle)))
                return rc;
            if ((rc = pageHandle.GetPageNum(curPageNum)))
                return rc;
            curSlotNum = 0;
        }
        // ���� GetNextRec() ȣ�⿡�� ��ü �������� ó������ �ʾҴٸ�
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

        // ��ĵ ������ ������� ���� ���ڵ带 ã��
        FindNextRecInCurPage(pData);

        // �� �������� ��ġ�ϴ� �׸��� ���ٸ� ���� �������� �̵�
        if (curSlotNum == pFileHandle->fileHdr.numRecordsPerPage) {
            if ((rc = pFileHandle->pfFileHandle.UnpinPage(curPageNum)))
                return rc;
            
            needRepeat = true;
        }

    } while (needRepeat);

    // ��ġ: �־��� ��ġ�� ���ڵ� ����
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

    // �� �������� �� �̻� ��ġ�ϴ� ���ڵ尡 ���ٸ�, ���� GetNextRec() ȣ�⿡�� �� �������� �׼����� �ʿ䰡 ����
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

        // �� ������ �ǳʶڴ�
        if (!pFileHandle->GetBitmap(pData + sizeof(RM_PageHdr), curSlotNum))
            continue;
        
        // compOp�� NO_OP�̸� �ٷ� ����
        if (compOp == NO_OP)
            break;

        // �Ӽ� ������ ���� �񱳸� �����Ѵ�
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

        // �� �����ڿ� ���� ������ ������
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
    // ����: 'this'�� �ݵ�� ���� �־�� ��
    if (!bScanOpen)
        return (RM_CLOSEDSCAN);

    // ��� ���� �ʱ�ȭ
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

    // ���������� ��ȯ
    return (0);
}