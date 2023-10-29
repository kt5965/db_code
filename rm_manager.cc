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

   // ����: recordSize�� �ʹ� ũ�ų� ���� �ʾƾ� ��
   // PF_Manager::CreateFile()�� fileName�� ó���� ����
   if (recordSize >= PF_PAGE_SIZE - sizeof(RM_PageHdr) || recordSize < 1)
      // �׽�Ʈ: �߸��� recordSize
      return (RM_INVALIDRECSIZE);

   // PF_Manager::CreateFile() ȣ��
   if ((rc = pPfm->CreateFile(fileName)))
   {
      pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
      return(rc);
   }

   // PF_Manager::OpenFile() ȣ��
   if ((rc = pPfm->OpenFile(fileName, pfFileHandle)))
   {
      pPfm->DestroyFile(fileName);
      return(rc);
   }
   // ��� ������ �Ҵ� (pageNum�� 0�̾�� ��)
   if ((rc = pfFileHandle.AllocatePage(pageHandle))) {
   {
      pPfm->CloseFile(pfFileHandle);
      return(rc);
   }
   // ��� ������ �ۼ��� ������ ���
   if ((rc = pageHandle.GetData(pData)))
   {
      pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
      return(rc);
   }

   // ���� ��� �ۼ� (���� Ǯ��)
   fileHdr = (RM_FileHdr *)pData;
   fileHdr->firstFree = RM_PAGE_LIST_END;
   fileHdr->recordSize = recordSize;
   // �������� ���ڵ� �� -> PGSIZE - RM_PageHdr - 1 / recordsize + 1/8
   fileHdr->numRecordsPerPage = (PF_PAGE_SIZE - sizeof(RM_PageHdr) - 1) 
                                / (recordSize + 1.0/8);
   if (recordSize * (fileHdr->numRecordsPerPage + 1) 
      + fileHdr->numRecordsPerPage / 8 
      <= PF_PAGE_SIZE - sizeof(RM_PageHdr) - 1)
      fileHdr->numRecordsPerPage++;
   fileHdr->pageHeaderSize = sizeof(RM_PageHdr) + (fileHdr->numRecordsPerPage + 7) / 8;
   fileHdr->numRecords = 0;

   // ��� �������� ��Ƽ ���·� ǥ��
   if ((rc = pfFileHandle.MarkDirty(RM_HEADER_PAGE_NUM)))
   {
      pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
      return(rc);
   }

   
   // ��� ������ ����
   if ((rc = pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM)))
   {
      pPfm->CloseFile(pfFileHandle);
      return(rc);
   }
   
   // PF_Manager::CloseFile() ȣ��
   if ((rc = pPfm->CloseFile(pfFileHandle)))
   {
      pPfm->DestroyFile(fileName);
      return(rc);
   }

   // ���� ��ȯ
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

   // PF_Manager::OpenFile() ȣ��
   rc = pPfm->OpenFile(fileName, fileHandle.pfFileHandle);
   if (rc) {
      return rc;
   }

   // ��� ������ ��������
   rc = fileHandle.pfFileHandle.GetFirstPage(pageHandle);
   if (rc) {
      pPfm->CloseFile(fileHandle.pfFileHandle);
      return rc;
   }

   // ��� ������ �ִ� ������ ���
   rc = pageHandle.GetData(pData);
   if (rc) {
      fileHandle.pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
      pPfm->CloseFile(fileHandle.pfFileHandle);
      return rc;
   }

   // ���� ��� �б� (���� Ǯ���� RM_FileHandle��)
   memcpy(&fileHandle.fileHdr, pData, sizeof(fileHandle.fileHdr));

   // ��� ������ ����
   rc = fileHandle.pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
   if (rc) {
      pPfm->CloseFile(fileHandle.pfFileHandle);
      return rc;
   }
   // ���� ����� ������� ���� ���·� ����
   fileHandle.bHdrChanged = FALSE;
   return (0);
}


RC RM_Manager::CloseFile(RM_FileHandle &fileHandle)
{
   RC rc;
   
   // ������ ���� �ִ� ���� ����� ��������� �־��ٸ� ���� ����� �ٽ� �ۼ�
   if (fileHandle.bHdrChanged) {
      PF_PageHandle pageHandle;
      char* pData;

      // ��� ������ ��������
      rc = fileHandle.pfFileHandle.GetFirstPage(pageHandle);
      if (rc) {
         return rc;
      }

      // ��� ������ �ۼ��� ������ ���
      rc = pageHandle.GetData(pData);
      if (rc) {
         fileHandle.pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
         return rc;
      }

      // ���� ��� �ۼ� (���� Ǯ��)
      memcpy(pData, &fileHandle.fileHdr, sizeof(fileHandle.fileHdr));

      // ��� �������� ��Ƽ�� ǥ��
      rc = fileHandle.pfFileHandle.MarkDirty(RM_HEADER_PAGE_NUM);
      if (rc) {
         fileHandle.pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
         return rc;
      }

      // ��� ������ ����
      rc = fileHandle.pfFileHandle.UnpinPage(RM_HEADER_PAGE_NUM);
      if (rc) {
         return rc;
      }

      // ���� ��� ������� ���� ���·� ����
      fileHandle.bHdrChanged = FALSE;
   }

   // PF_Manager::CloseFile() ȣ��
   rc = pPfm->CloseFile(fileHandle.pfFileHandle);
   if (rc) {
      return rc;
   }

   // ��� ���� �缳��
   memset(&fileHandle.fileHdr, 0, sizeof(fileHandle.fileHdr));
   fileHandle.fileHdr.firstFree = RM_PAGE_LIST_END;

   // ���� ��ȯ
   return OK_RC;
}
