#include "rm_internal.h"

RM_Record::RM_Record()
{
   pData = NULL;   // 데이터 포인터 초기화
   recordSize = 0; // 레코드 크기 초기화
}

RM_Record::~RM_Record()
{
}

RC RM_Record::GetData(char *&_pData) const
{
   // 레코드가 읽혀져야 함
   if (pData == NULL)
      return (RM_UNREADRECORD);

   // 이 RM_Record의 데이터로 매개변수 설정
   _pData = pData;
   return (0);
}

RC RM_Record::GetRid(RID &_rid) const
{
   // 레코드가 읽혀져야 함
   if (pData == NULL)
      return (RM_UNREADRECORD);

   // 이 RM_Record의 레코드 식별자로 매개변수 설정
   _rid = rid;

   return (0);
}

RC RM_Record::SetData(char *pData2, int size, RID rid_)
{
   recordSize = size;      // 레코드 크기 설정
   this->rid = rid_;       // 레코드 식별자 설정
   if (pData == NULL)      // pData가 아직 할당되지 않았다면
      pData = new char[recordSize]; // 메모리 할당
   memcpy(pData, pData2, size); // 데이터 복사
   
   return 0;
}