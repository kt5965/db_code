#include <stdio.h>
#include <iostream>
#include "pf.h"
#include <stdlib.h>
#include <string.h>
#include <rm_rid.h>
#include "rm.h"
#include "stddef.h"
typedef struct record {
    char name[32];
    char addr[64];
    char telnum[32];
    char email[32];
} RecordT;

// pData에 레코드 삽입
void recordTo_pData(RecordT record, char *pData) {
    sprintf(pData, "%s, %s, %s, %s", record.name, record.addr, record.telnum, record.email);
}


// pData로 들어온 레코드 값을 person에 삽입 후 리턴
RecordT pDataToRecord(char *pData) {
    RecordT person;
    if (pData == NULL) {
        strcpy(person.name, "ERROR");
        strcpy(person.addr, "ERROR");
        strcpy(person.telnum, "ERROR");
        strcpy(person.email, "ERROR");
        return person;
    }
    char *temp = (char *)malloc(4096);
    strncpy(temp, pData, 4096);
    strcpy(person.name, strtok(temp, ","));
    strcpy(person.addr, strtok(NULL, ","));
    strcpy(person.telnum, strtok(NULL, ","));
    strcpy(person.email, strtok(NULL, ","));
    free(temp);
    return person;
}

int menuDisplay();
int deleteContact(PF_FileHandle &fh);
int insertContact(PF_FileHandle &fh);
int searchContact(PF_FileHandle &fh);
int updateContact(PF_FileHandle &fh);
int showAllContact(PF_FileHandle &fh);
int findPage(char *name, PF_FileHandle &fh, PF_PageHandle &ph, char* &pData);
void showBuffer(PF_Manager &pfm);
int insert40Contacts(PF_FileHandle &fh); 
int getNumPages(PF_FileHandle &fh);


int main() {
    PF_Manager pfm;
    PF_FileHandle fh;
    PF_PageHandle ph;
    RC rc;
    int select;
    // createFile 없으면 생성
    if (rc = pfm.CreateFile("Contactlist.dat")) {
        printf("\nDataFile is already exist!");
    }
    // 파일 오픈
    if ((rc = pfm.OpenFile("Contactlist.dat", fh))) {
        printf("DataFile open error!\n");
        return (rc);
    }

    system("clear");

    // menu 고름
    while ((select = menuDisplay()) != 8) {
        switch (select) {
            case 1:
                insertContact(fh);
                break;
            case 2:
                deleteContact(fh);
                break;
            case 3:
                updateContact(fh);
                break;
            case 4:
                searchContact(fh);
                break;
            case 5:
                showAllContact(fh);
                break;
            case 6:
                insert40Contacts(fh);
                break;
            case 7:
                showBuffer(pfm);
                break;
            default:
                break;
        }
    }

    pfm.CloseFile(fh);

    return 0;
}

// 버퍼 전부 보여주기
void showBuffer(PF_Manager &pfm){
    system("clear");
    pfm.PrintBuffer();
    printf("\nPress Enter key to go back menu screen\n");
    getchar();
    getchar();
}


// contacts 한 번에 넣기
// 버퍼 교체 알고리즘 변경을 위해 구현
int insert40Contacts(PF_FileHandle &fh) {
    RC rc;
    char * pData;
    PageNum pageNum;
    PF_PageHandle ph;
    RecordT person;
    // pageNum 받아와서 40개가 넘는지 확인  
    int numpages = getNumPages(fh);
    if(numpages >= 40) {
        system("clear");
        printf("alread exist 40 Pages\n");
        getchar();
        getchar();
        return 0;
    }
    // 40개 넣기
    for (int i = 0; i < 40; i++) {
        sprintf(person.name, "test%d", i);
        sprintf(person.addr, "test%d", i);
        sprintf(person.telnum, "test%d", i);
        sprintf(person.email, "test%d", i);
        getchar();
        // 페이지 할당 후 pData를 ph에 할당하고 pageNum 할당  
        if ((rc = fh.AllocatePage(ph)) || (rc = ph.GetData(pData)) || (rc = ph.GetPageNum(pageNum))) {
            return rc;
        }
        // person에 입력받은 값 pData로 넘겨줌
        recordTo_pData(person, pData);
        // 페이지 더티 및 언핀해줌
        fh.MarkDirty(pageNum);
        fh.UnpinPage(pageNum);
    }
    system("clear");
    printf("insert 40 pages\n");
    getchar();
    getchar();
    return 0;
}

// 메뉴 디스플레이
int menuDisplay() {
	int select;

	system("clear");
	printf("\n  CONTACT MANAGER\n");
	printf("=====================\n");
	printf("    1. Insert\n");
	printf("    2. Delete\n");
	printf("    3. Update\n");
	printf("    4. Search\n");
	printf("    5. Show All\n");
	printf("    6. Insert 40 Records\n");
	printf("    7. Show Buffers\n");
	printf("    8. Exit\n");
	printf("=====================\n");
	printf("Press Menu Number>>");

	select = getchar() - 48;
	system("clear");

	return select;
}

// 저장된 모든 데이터 보이기
int showAllContact(PF_FileHandle &fh) {
    PF_PageHandle ph;
    PageNum pageNum;
    char *pData;
    RC rc;
    RecordT person;
    // totla page 갯수만큼 확인
    int totalPages = getNumPages(fh);
    getchar();
    // page 돌면서
    for (int pageNum = 0; pageNum < totalPages+1; pageNum++) {
        // 페이지 버퍼에서 페이지 넘버 가져와서 핸들에 할당
        rc = fh.GetThisPage(pageNum, ph);
        if (rc != 0) {
            fh.UnpinPage(pageNum);
            getchar();
            return -1;
        }
        // 페이지 핸들러의 데이터를 pData에 저장해줌
        rc = ph.GetData(pData);
        if (rc != 0) {
            fh.UnpinPage(pageNum);
            getchar();
            return -1;
        }

        person = pDataToRecord(pData);
        printf("---------------\n");
        printf("%s\n", person.name);
        printf("Update addr : ");
        printf("%s\n", person.addr);
        printf("Update telnum : ");
        printf("%s\n", person.telnum);
        printf("Update email : ");
        printf("%s\n", person.email);
        printf("---------------\n");
        fh.UnpinPage(pageNum);
    }
    getchar();
    getchar();
    return 0;
}

// 데이터 업데이트
int updateContact(PF_FileHandle &fh) {
    PageNum pageNum;
    char person_name[32];
    PF_PageHandle ph;
    RC rc;
    char *pData;
    RecordT person;
    printf("Input a name that you want to update\n");
    scanf("%s", person_name);

    // file에서 person_name을 찾아서 pageNum return해줌
    pageNum = findPage(person_name, fh, ph, pData);
    if (pageNum == -1) {
        printf("\nYour Information doesn't exist\n");
        getchar();
        return -1;
    }
    // pData에 ph의 데이터 넣어줌
    rc = ph.GetData(pData);
    if (rc != 0) {
        printf("no data\n");
        return -1;
    }

    getchar();
    printf("Update name : ");
    scanf("%s", person.name);
    printf("Update addr : ");
    scanf("%s", person.addr);
    printf("Update telnum : ");
    scanf("%s", person.telnum);
    printf("Update email : ");
    scanf("%s", person.email);
    // pData 업데이트 해줌
    recordTo_pData(person, pData);
    // pageNum 언핀해줌
    // fh.MarkDirty(pageNum);
    fh.UnpinPage(pageNum);
    getchar();
}

// 데이터 삭제
int deleteContact(PF_FileHandle &fh) {
    char person_name[32];
    RC rc;
    PageNum pageNum;
    PF_PageHandle ph;
    int isFound = 0;
    char *pData;
    RecordT person;

    printf("Input a name that you want to delete\n");
    scanf("%s", person_name);
    // Page 찾기 해당 이름이 저장된
    pageNum = findPage(person_name, fh, ph, pData);
    if (pageNum == -1) {
        printf("\nYour Information doesn't exist\n");
        getchar();
        return -1;
    }
    getchar();
    printf("%d\n", pageNum);
    getchar();
    printf("asdasd\n");
    rc = fh.UnpinPage(pageNum);
    printf("asdasd2\n");
    if (rc != OK_RC) {
        printf("UnpinPage error, code %d\n", rc);
        getchar();
        return rc;
    }
    rc = fh.DisposePage(pageNum);
    if (rc != 0) {
        printf("DisposePage error, code %d\n", rc);
        getchar();
        return rc;
    }
    getchar();
    getchar();
    fh.FlushPages();
    return 0;
}

int insertContact(PF_FileHandle &fh) {
    RC rc;
    char *pData;
    PageNum pageNum;
    PF_PageHandle ph;
    RecordT person;

    printf("name : ");
    scanf("%s", person.name);
    printf("addr : ");
    scanf("%s", person.addr);
    printf("telnum : ");
    scanf("%s", person.telnum);
    printf("email : ");
    scanf("%s", person.email);

    if ((rc = fh.AllocatePage(ph)) || (rc = ph.GetData(pData)) || (rc = ph.GetPageNum(pageNum))) return (rc);
    printf("pagenum = %d\n", pageNum);
    recordTo_pData(person, pData);
    fh.MarkDirty(pageNum);
    fh.UnpinPage(pageNum);
    printf("\nPress Enter key to go back menu screen\n");
    getchar();
    getchar();

    return 0;
}

int searchContact(PF_FileHandle &fh) {
    PF_PageHandle ph;
    char person_name[32];
    RecordT person;
    char * pData;
    PageNum pageNum;

    printf("Input a name that you want to search\n");
    scanf("%s", person_name);
    pageNum = findPage(person_name, fh, ph, pData);
    if (pageNum == -1) {
        printf("\nYour Information doesn't exist\n");
        getchar();
        return -1;
    }


    person = pDataToRecord(pData);
    if (person.name == "ERROR") {
        printf("no data here\n");
    }
    printf("\n     The result you want\n");
    printf("=================================\n");
    printf("name      : %s\n", person.name);
    printf("address   : %s\n", person.addr);
    printf("contact   : %s\n", person.telnum);
    printf("mail      : %s\n", person.email);
    printf("=================================\n");
    printf("\npress enter key to go back menu screen\n");
    fh.UnpinPage(pageNum);
    getchar();
    getchar();

    return 0;
}

int findPage(char *name, PF_FileHandle &fh, PF_PageHandle &ph, char* &pData) {
    PageNum pageNum;
    RC rc;
    RecordT person;
    int totalPages = getNumPages(fh);
    printf("total pages = %d\n", totalPages);
    getchar();
    for (pageNum = 0; pageNum < totalPages+1; pageNum++)
    {
        rc = fh.GetThisPage(pageNum, ph);
        if (rc != 0) {
            fh.UnpinPage(pageNum);
            printf("1");
            getchar();
            return -1;
        }

        rc = ph.GetData(pData);

        if (rc != 0) {
            fh.UnpinPage(pageNum);
            printf("2");
            getchar();
            return -1;
        }

        person = pDataToRecord(pData);

        if (strcmp(person.name, name) == 0) {
            getchar();
            fh.IncreaseUseCount(pageNum);
            return pageNum;
        }
        fh.UnpinPage(pageNum);
    }
    return -1;
}

int getNumPages(PF_FileHandle &fh)
{
    PF_PageHandle ph;
    PageNum pageNum;
    int numPages = 1;

    if (fh.GetFirstPage(ph) == 0) {
        numPages++;
        while (fh.GetNextPage(numPages, ph) == 0) {
            numPages++;
            pageNum++;
        }
        fh.UnpinPage(pageNum);
    }
    
    return numPages;
}