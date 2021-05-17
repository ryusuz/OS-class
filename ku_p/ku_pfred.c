#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <mqueue.h>
#include <fcntl.h>
#include <malloc.h>
#include <memory.h>
#include <sys/wait.h>

#define MSG_SIZE    4

int main(int argc, char* argv[]){
    int fd;                                                 //dataset 파일 descriptor
    char buf[5];                                            //리드한 데이터값 저장할 버퍼
    int *data;                                              //캐릭터형 데이터를 인트형 데이터로 변환해서 저장할 버퍼
    char numcheck[6];                                       //파일의 맨 위(데이터 개수) 숫자 넣을 배열
    int count;                                              //읽을 바이트 수
    int start = 0;                                          //데이터의 처음위치를 저장할 변수
    int total = 0;                                          //파일에 있는 데이터의 총 개수
    int pnum = atoi(argv[1]);                               //사용자에게 입력받을 프로세스 개수
    int interval = atoi(argv[2]);                           //사용자에게 입력받을 범위
    int int_size = 10000 / interval;                        //범위의 개수, 데이터가 0000 ~ 9999까지 존재하기 때문에 10000에서 범위를 나눠줌
    int *result;                                            //최종 결과 : 도수분포 배열
    int i, j;                                               //반복문에 쓰일 변수
    int pdata;                                              //각 프로세스가 받을 데이터 개수
    int index = 0;                                          //data 배열의 인덱스 번호
    int re_idx = 0;                                         //result 배열의 인덱스 번호인데 각 인덱스 번호는 범위
    struct mq_attr attr;
    mqd_t *mqdes = (mqd_t *)malloc(sizeof(mqd_t)*pnum);     //메세지큐 프로세스 개수만큼 생성해서
    unsigned int prio = int_size + 1;                       //우선순위 감소시키려고
    char name[256];                                         //메세지큐 이름의 크기
    int child_status;
    int *tmp;                                               //메세지큐에서 받아오기 위한 템퍼
    int rest = 0;
    int rest2 = 0;

    //파일 오픈
    if((fd = open(argv[3], O_RDWR)) == -1) {                
        perror("open()\n");
        exit(0); 
    }

    do {                                        //데이터셋 생성기 실행할 때 입력했던 수(데이터 개수) 확인
        pread(fd, numcheck, 1, start++);        //start에 개행문자 다음의 위치, 즉 실제 데이터의 위치가 저장됨
    } while (numcheck[0] != '\n');              //개행문자가 나오면 반복문 종료. 개행문자 전까지가 데이터의 개수

    pread(fd, numcheck, start, 0);              //파일의 처음 위치부터 start(개행문자 위치)까지 numcheck에 리드해줌
    total = atoi(numcheck);                     //numcheck에 들어간 수는 캐릭터형이므로 인트형으로 변환해줌
    memset(numcheck, 0, 6);                    //초기화

    rest = total % pnum;                        //나머지 저장
    pdata = total / pnum;                       //각 프로세스 당 데이터 개수 = 전체 데이터/프로세스 개수
    count = pdata * 5;                          //각 프로세스 당 읽을 데이터 바이트 수 = 각 데이터 개수 * (숫자 4바이트 + 개행문자 1바이트)
    data = (int *)malloc(sizeof(int) * pdata);  //각 프로세스에서 맡은 데이터 개수만큼 인트형 배열 동적할당
    //printf("rest : %d, pdata: %d, count %d \n", rest, pdata, count);
    //printf("total : %d \n", total);

    rest2 = 10000 % interval;                   //나머지가 생기면 범위가 추가되어야함
    if (rest2 != 0) {
        int_size += 1;                          //범위의 개수에 +1 해줌
    }
    //printf("rest2 : %d, interval \n", rest2);
    result = (int *)malloc(sizeof(int) * int_size);
    tmp = (int *)malloc(sizeof(int) * int_size);

    attr.mq_maxmsg = 10;                        //메세지큐 크기 = 범위의 개수
    attr.mq_msgsize = MSG_SIZE;

    //메세지큐 오픈
    for (i = 0; i < pnum; i++) {
        sprintf(name, "/mq_name%d", i);
        mqdes[i] = mq_open(name, O_CREAT | O_RDWR, 0666, &attr);

        if(mqdes[i] < 0) {
            perror("mq_open()\n");
            exit(0);
        }
    }

    //입력받은 pnum만큼 자식 프로세스 생성
    for (i = 0; i < pnum; i++) {
        if (fork() == 0) {
            start = start + (count * i);        //각 프로세스별 시작 위치.
            if(i == pnum - 1) {                 //마지막 프로세스에 나머지 다 넣음
                pdata += rest;
                count = pdata * 5;
                //printf("pnum - 1 프로세스 pdata : %d, count :%d\n", pdata, count);
            }

            for (j = 0; j < count; j += 5) {     //개행문자 포함 5바이트씩 버퍼에 저장 후 인트형으로 바꿔서 data에 저장
                pread(fd, buf, 5, start + j);
                data[index++] = atoi(buf);
            }

            for (j = 0; j < pdata; j++) {
                re_idx = data[j]/interval;      //데이터와 범위를 나누면 해당 데이터가 속한 범위를 알 수 있음
                result[re_idx] += 1;
            }
            

            for ( j = 0; j < int_size; j++) {
                if(mq_send(mqdes[i], (char *)&result[j], MSG_SIZE, prio--) == -1) {
                    perror("mq_send()\n");
                    exit(0);
                  }
            }

            exit(0);

        }
    }

    //자식이 보낸 결과값 부모가 리시브
    for( i = 0; i < pnum; i++) {
        for (j = 0; j < int_size; j++) {
            if (mq_receive(mqdes[i], (char *)&tmp[j], MSG_SIZE, &prio) == -1) {
                perror("mq_receive()\n");
                exit(0);
            }
            result[j] += tmp[j];
        }
        wait(&child_status);
        mq_close(mqdes[i]);
        sprintf(name, "/mq_name%d", i);
        mq_unlink(name);
    }

    for (i = 0; i < int_size; i++) {
        printf("%d\n", result[i]);
    }
   
    close(fd);
    free(data);
    free(mqdes);
    free(result);
    free(tmp);

    return 0;
}