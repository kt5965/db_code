#include "rm_internal.h"

// 기본 생성자
RID::RID()
{
   pageNum = 0;
   slotNum = 0;
}

// 파라미터를 가지는 생성자
RID::RID(PageNum _pageNum, SlotNum _slotNum)
{
   pageNum = _pageNum;
   slotNum = _slotNum;
}

// 복사 생성자
RID::RID(const RID &rid)
{
    pageNum = rid.pageNum;
    slotNum = rid.slotNum;
}

// 소멸자
RID::~RID()
{
}

// 대입 연산자 오버로딩
RID& RID::operator= (const RID &rid)
{
   // 자기 자신에 대한 대입인지 확인
   if (this != &rid) {
      // 메모리 할당이 없으므로 멤버 변수만 복사
      this->pageNum = rid.pageNum;
      this->slotNum = rid.slotNum;
   }

   // this의 참조를 반환
   return (*this);
}

// 동등성 연산자 오버로딩
bool RID::operator==(const RID &rid) const
{
   // 페이지 번호와 슬롯 번호가 모두 같은지 확인
   return (this->pageNum == rid.pageNum) && (this->slotNum == rid.slotNum);
}

// 페이지 번호 얻기
RC RID::GetPageNum(PageNum &_pageNum) const
{
   // 유효한 레코드 식별자인지 확인
   if (pageNum == 0)
      return (RM_INVIABLERID);

   // 파라미터로 이 RID의 페이지 번호를 설정
   _pageNum = pageNum;    

   // 정상적으로 리턴
   return (0);
}

// 슬롯 번호 얻기
RC RID::GetSlotNum(SlotNum &_slotNum) const
{
   // 유효한 레코드 식별자인지 확인
   if (pageNum == 0)
      return (RM_INVIABLERID);

   // 파라미터로 이 RID의 슬롯 번호를 설정
   _slotNum = slotNum;    

   // 정상적으로 리턴
   return (0);
}