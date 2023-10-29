#include "rm_internal.h"

RM_Record::RM_Record()
{
   pData = NULL;   // ������ ������ �ʱ�ȭ
   recordSize = 0; // ���ڵ� ũ�� �ʱ�ȭ
}

RM_Record::~RM_Record()
{
}

RC RM_Record::GetData(char *&_pData) const
{
   // ���ڵ尡 �������� ��
   if (pData == NULL)
      return (RM_UNREADRECORD);

   // �� RM_Record�� �����ͷ� �Ű����� ����
   _pData = pData;
   return (0);
}

RC RM_Record::GetRid(RID &_rid) const
{
   // ���ڵ尡 �������� ��
   if (pData == NULL)
      return (RM_UNREADRECORD);

   // �� RM_Record�� ���ڵ� �ĺ��ڷ� �Ű����� ����
   _rid = rid;

   return (0);
}

RC RM_Record::SetData(char *pData2, int size, RID rid_)
{
   recordSize = size;      // ���ڵ� ũ�� ����
   this->rid = rid_;       // ���ڵ� �ĺ��� ����
   if (pData == NULL)      // pData�� ���� �Ҵ���� �ʾҴٸ�
      pData = new char[recordSize]; // �޸� �Ҵ�
   memcpy(pData, pData2, size); // ������ ����
   
   return 0;
}