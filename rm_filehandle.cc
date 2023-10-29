#include "rm_internal.h"


RM_FileHandle::RM_FileHandle()
{
   // ���� �ڵ鷯�� ��Ƽ�������� üũ
   bHdrChanged = FALSE;
   // fileHdr ����ü ���κ��� �ʱ�ȭ
   memset(&fileHdr, 0, sizeof(fileHdr));
   // �ƹ��� �������� ��밡������ ����
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

   // rid���� ������ ��ȣ ����
   if ((rc = rid.GetPageNum(pageNum)))
      return (rc);

   // rid���� ���� ��ȣ ����
   if ((rc = rid.GetSlotNum(slotNum)))
      return (rc);

   // slotNum ���� �˻�
   // PF_FileHandle.GetThisPage()�� pageNum�� ó���� ����
   if (slotNum >= fileHdr.numRecordsPerPage || slotNum < 0)
      return (RM_INVALIDSLOTNUM);
   
   // rid�� ����Ű�� ������ ��������
   if ((rc = pfFileHandle.GetThisPage(pageNum, pageHandle)))
      return (rc);

   // ������ ��������
   if ((rc = pageHandle.GetData(pData)))
      pfFileHandle.UnpinPage(pageNum);

   // rid�� �ش��ϴ� ���ڵ尡 �����ؾ� ��
   if (!GetBitmap(pData + sizeof(RM_PageHdr), slotNum)) {
      pfFileHandle.UnpinPage(pageNum);
      return (RM_RECORDNOTFOUND);
   }

   // RM_Record�� ���ڵ� ����
   rec.rid = rid;
   if (rec.pData)
      delete [] rec.pData;
   rec.recordSize = fileHdr.recordSize;
   rec.pData = new char[rec.recordSize];
   memcpy(rec.pData,
          pData + fileHdr.pageHeaderSize + slotNum * fileHdr.recordSize, 
          fileHdr.recordSize);

   // ������ ����
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

   // pRecordData�� NULL�̸� �ȵ�
   if (pRecordData == NULL)
      return (RM_NULLPOINTER);
 
   // ���� ������ ����� ��� ������ �� ������ �Ҵ�
   if (fileHdr.firstFree == RM_PAGE_LIST_END) {
      // PF_FileHandle::AllocatePage() ȣ��
      if ((rc = pfFileHandle.AllocatePage(pageHandle)))
         return rc;

      // ������ ��ȣ ���
      if ((rc = pageHandle.GetPageNum(pageNum)))
         return rc;

      // ������ ������ ���
      if ((rc = pageHandle.GetData(pData)))
         return rc;

      // ������ ��� ����
      ((RM_PageHdr *)pData)->nextFree = RM_PAGE_LIST_END;

      // ���� �����͸� ���������Ƿ� �������� ��Ƽ ǥ��
      if ((rc = pfFileHandle.MarkDirty(pageNum)))
         return rc;

      // ������ ����
      if ((rc = pfFileHandle.UnpinPage(pageNum)))
         return rc;

      // ���� ������ ��Ͽ� �ֱ�
      fileHdr.firstFree = pageNum;
      bHdrChanged = TRUE;
   }
   // ��Ͽ��� ù ��° ������ ����
   else {
      pageNum = fileHdr.firstFree;
   }

   // �� ���ڵ尡 ���Ե� ������ ��
   if ((rc = pfFileHandle.GetThisPage(pageNum, pageHandle)))
      return rc;

   // ������ ������ ���
   if ((rc = pageHandle.GetData(pData)))
      return rc;

   // ��� �ִ� ���� ã��
   for (slotNum = 0; slotNum < fileHdr.numRecordsPerPage; slotNum++)
      if (!GetBitmap(pData + sizeof(RM_PageHdr), slotNum))
         break;

   // ���� ������ �־�� ��
   assert(slotNum < fileHdr.numRecordsPerPage);
   
   // rid �Ҵ�
   pRid = new RID(pageNum, slotNum);
   rid = *pRid;
   delete pRid;

   // �־��� ���ڵ� �����͸� ���� Ǯ�� ����
   memcpy(pData + fileHdr.pageHeaderSize + slotNum * fileHdr.recordSize, 
          pRecordData, fileHdr.recordSize);

   // ��Ʈ ����
   SetBitmap(pData + sizeof(RM_PageHdr), slotNum);

   // ��� �ִ� ���� ã��
   for (slotNum = 0; slotNum < fileHdr.numRecordsPerPage; slotNum++)
      if (!GetBitmap(pData + sizeof(RM_PageHdr), slotNum))
         break;

   // �ʿ��� ��� ���� ������ ��Ͽ��� ������ ����
   if (slotNum == fileHdr.numRecordsPerPage) {
      fileHdr.firstFree = ((RM_PageHdr *)pData)->nextFree;
      bHdrChanged = TRUE;
      ((RM_PageHdr *)pData)->nextFree = RM_PAGE_FULL;
   }

   // ��Ʈ���� �����߱� ������ ��� �������� ��Ƽ ǥ��
   if ((rc = pfFileHandle.MarkDirty(pageNum)))
      return rc;

   // ������ ����
   if ((rc = pfFileHandle.UnpinPage(pageNum)))
      return rc;

   // ���� ��ȯ
   return (0);
}

RC RM_FileHandle::DeleteRec(const RID &rid)
{
   RC rc;
   PageNum pageNum;
   SlotNum slotNum;
   PF_PageHandle pageHandle;
   char *pData;

   // rid���� ������ ��ȣ ����
   if (rc = rid.GetPageNum(pageNum))
      return rc;

   // rid���� ���� ��ȣ ����
   if (rc = rid.GetSlotNum(slotNum))
      return rc;

   // ����Num ��� Ȯ��
   if (slotNum >= fileHdr.numRecordsPerPage || slotNum < 0)
      return (RM_INVALIDSLOTNUM);
   
   if (rc = pfFileHandle.GetThisPage(pageNum, pageHandle))
      return rc;

   if (rc = pageHandle.GetData(pData)) {
      pfFileHandle.UnpinPage(pageNum);
      return rc;
   }

   if (!GetBitmap(pData + sizeof(RM_PageHdr), slotNum)) { 
      // �ش� rid�� �ش��ϴ� ���ڵ尡 �����ؾ� ��
      pfFileHandle.UnpinPage(pageNum);
      return (RM_RECORDNOTFOUND);
   }
   // ��Ʈ �����
   ClrBitmap(pData + sizeof(RM_PageHdr), slotNum);
   
   memset(pData + fileHdr.pageHeaderSize + slotNum * fileHdr.recordSize, 
          '\0', fileHdr.recordSize);
   // �� ���� ã��
   for (slotNum = 0; slotNum < fileHdr.numRecordsPerPage; slotNum++)
      if (GetBitmap(pData + sizeof(RM_PageHdr), slotNum))
         break;

   // �������� ��� ������ (������ ���ڵ尡 �������̾�����) �������� �����Ѵ�.
   // �̷��� �ϸ� �����ϰ� �ִ� �������� �� ���� �ִ��� �۰� ������ �� �ִ�.
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
      // �������� ���� ������ ��Ͽ� �߰�
      ((RM_PageHdr *)pData)->nextFree = fileHdr.firstFree;
      fileHdr.firstFree = pageNum;
      bHdrChanged = TRUE;
   }

   // ��Ʈ���� ����Ǿ����Ƿ� ��� �������� ��Ƽ ���·� ǥ��
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

   // RID ���
   if ((rc = rec.GetRid(rid)))
   {
      return rc;
   }

   // ���ڵ� ������ ���
   if ((rc = rec.GetData(pRecordData)))
   {
      return rc;
   }

   // rid���� ������ ��ȣ ����
   if ((rc = rid.GetPageNum(pageNum)))
   {
      return rc;
   }

   // rid���� ���� ��ȣ ����
   if ((rc = rid.GetSlotNum(slotNum)))
   {
      return rc;
   }

   // ����Num ��� �˻�
   if (slotNum >= fileHdr.numRecordsPerPage || slotNum < 0)
   {
      return RM_INVALIDSLOTNUM;
   }

   // ������Ʈ�ϴ� ���ڵ�� ���� �ڵ��� recordSize�� ��ġ�ؾ� ��
   if (rec.recordSize != fileHdr.recordSize)
   {
      return RM_INVALIDRECSIZE;
   }

   // rid�� ����Ű�� ������ ���
   if ((rc = pfFileHandle.GetThisPage(pageNum, pageHandle)))
   {
      return rc;
   }

   // ������ ���
   if ((rc = pageHandle.GetData(pData)))
   {
      pfFileHandle.UnpinPage(pageNum);
      return rc;
   }

   // rid�� �ش��ϴ� ���ڵ尡 �����ؾ� ��
   if (!GetBitmap(pData + sizeof(RM_PageHdr), slotNum))
   {
      pfFileHandle.UnpinPage(pageNum);
      return RM_RECORDNOTFOUND;
   }

   // �־��� ���ڵ��� ������(pRecordData)�� ������ ���� ������ ��ġ(pData + fileHdr.pageHeaderSize + slotNum * fileHdr.recordSize)�� ����
   // ������ ���� ��ġ+������ ��� ũ�� + slotNum*���ڵ� ũ���� ��ġ�� precorddata�� ����
   memcpy(pData + fileHdr.pageHeaderSize + slotNum * fileHdr.recordSize, pRecordData, fileHdr.recordSize);

   // ��� �������� ��Ƽ ���·� ǥ��
   if ((rc = pfFileHandle.MarkDirty(pageNum)))
   {
      pfFileHandle.UnpinPage(pageNum);
      return rc;
   }

   // ������ ����
   if ((rc = pfFileHandle.UnpinPage(pageNum)))
   {
      return rc;
   }
 
   return OK_RC;
}

RC RM_FileHandle::ForcePages(PageNum pageNum)
{
    RC rc;

    // ���� ����� ������ �߻��� ���, ������ ���� �� ����� ���� ������ �ۼ�
    if (bHdrChanged) 
    {
        PF_PageHandle pageHandle;
        char* pData;

        // ��� �������� �����´�.
        if (rc = pfFileHandle.GetFirstPage(pageHandle))
            return rc; 

        // ��� ������ �ۼ��� �����͸� ��´�.
        if (rc = pageHandle.GetData(pData))
        {
            pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
            return rc;
        }

        // ���� ����� ���� Ǯ�� ����.
        memcpy(pData, &fileHdr, sizeof(fileHdr));

        // ��� �������� ����� ������ ǥ���Ѵ�.
        if (rc = pfFileHandle.MarkDirty(RM_HEADER_PAGE_NUM))
        {
            pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
            return rc;
        }

        // ��� �������� ������ �����Ѵ�.
        if (rc = pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM))
            return rc;

        // ��� �������� ������ ��ũ�� ����Ѵ�.
        if (rc = pfFileHandle.ForcePages(RM_HEADER_PAGE_NUM))
            return rc;

        // ���� ����� ������� ���� ���·� �ǵ�����.
        bHdrChanged = FALSE;
    }

    // �־��� ������ ��ȣ�� �ش��ϴ� �������� ������ ��ũ�� ����Ѵ�.
    if (rc = pfFileHandle.ForcePages(pageNum))
        return rc;

    return 0;
}


// ����Ʈ �迭: [01010101, 10011001] 
// char Ÿ���� ����ϱ� ������ 8��Ʈ�� ǥ���� ��
int RM_FileHandle::GetBitmap(char *map, int idx) const
{
   return (map[idx / 8] & (1 << (idx % 8))) != 0;
}

// ���ϴ� ��ġ�� ��Ʈ�� 1�� �ٲٰ� or ����
void RM_FileHandle::SetBitmap(char *map, int idx) const
{
   map[idx / 8] |= (1 << (idx % 8));
}


void RM_FileHandle::ClrBitmap(char *map, int idx) const
{
   map[idx / 8] &= ~(1 << (idx % 8));
}

