#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <memory.h>
#include <fcntl.h>
#include <pthread.h>

int rank = 0;                               //몇번째에 생성된 스레드인지 파악하기 위한 변수                    
int *result;                                //최종 결과 : 도수분포 배열
pthread_mutex_t mutex;
pthread_mutex_t mutex2;

typedef struct __data {
    int start;                              //시작위치
    int tnum;                               //입력받을 스레드 개수
    int tdata;                              //스레드 당 데이터 개수
    int rest;                               //전체 데이터 개수 % 스레드 개수해서 나온 나머지 저장
    int count;                              //스레드당 읽을 데이터 크기
    int interval;                           //입력받을 범위
    int int_size;                           //범위 개수
    int fd;                                 
}Data;

void* run_data(void * arg) {
    Data *my = (Data *)arg;
    int start;
    int i;
    int *data;
    int re_idx = 0;
    int index = 0;
    char buf[5];                                            //리드한 데이터값 저장할 버퍼
    int rank2;
    int tdata = my->tdata;
    int count = my->count;

    pthread_mutex_lock(&mutex2);
    rank2 = rank++;
    pthread_mutex_unlock(&mutex2);

    if(rank2 == my->tnum - 1) {                 //마지막 스레드에 나머지 다 넣음
        tdata += my->rest;
        count = tdata * 5;
    }

    data = (int *)malloc(sizeof(int) * tdata);

    start = my->start + (count * rank2);
    //printf("rank : %d, pdata : %d, count :%d, start : %d\n", rank2, tdata, count, start);

    for (i = 0; i < count; i += 5) {            //개행문자 포함 5바이트씩 버퍼에 저장 후 인트형으로 바꿔서 data에 저장
        pread(my->fd, buf, 5, start + i);
        data[index++] = atoi(buf);
    }

    for (i = 0; i < tdata; i++) {
        re_idx = data[i]/my->interval;          //데이터와 범위를 나누면 해당 데이터가 속한 범위를 알 수 있음
        pthread_mutex_lock(&mutex);
        result[re_idx] += 1;
        pthread_mutex_unlock(&mutex);
     }

    free(data);
}

int main(int argc, char* argv[]){
    Data *data = (Data *)malloc(sizeof(Data));
    char numcheck[6];                                       //파일의 맨 위(데이터 개수) 숫자 넣을 배열
    int total = 0;                                          //파일에 있는 데이터의 총 개수
    data->tnum = atoi(argv[1]);                             //사용자에게 입력받을 프로세스 개수
    data->interval = atoi(argv[2]);                         //사용자에게 입력받을 범위
    data->int_size = 10000 / data->interval;                //범위의 개수, 데이터가 0000 ~ 9999까지 존재하기 때문에 10000에서 범위를 나눠줌
    int i, j;                                               //반복문에 쓰일 변수
    int rest2 = 0;
    pthread_t *tid = (pthread_t *)malloc(sizeof(pthread_t) * data->tnum);
    int check_tid;

    //파일 오픈
    if((data->fd = open(argv[3], O_RDWR)) == -1) {                
        perror("open()\n");
        exit(0); 
    }

    data->start = 0;
    do {                                                    //데이터셋 생성기 실행할 때 입력했던 수(데이터 개수) 확인
        pread(data->fd, numcheck, 1, data->start++);        //start에 개행문자 다음의 위치, 즉 실제 데이터의 위치가 저장됨
    } while (numcheck[0] != '\n');                          //개행문자가 나오면 반복문 종료. 개행문자 전까지가 데이터의 개수

    pread(data->fd, numcheck, data->start, 0);              //파일의 처음 위치부터 start(개행문자 위치)까지 numcheck에 리드해줌
    total = atoi(numcheck);                                 //numcheck에 들어간 수는 캐릭터형이므로 인트형으로 변환해줌
    memset(numcheck, 0, 6);                                 //초기화

    data->rest = total % data->tnum;                        //나머지 저장
    data->tdata = total / data->tnum;                       //각 프로세스 당 데이터 개수 = 전체 데이터/프로세스 개수
    data->count = data->tdata * 5;                          //각 프로세스 당 읽을 데이터 바이트 수 = 각 데이터 개수 * (숫자 4바이트 + 개행문자 1바이트)
    //printf("rest : %d, pdata: %d, count %d \n", rest, pdata, count);
    //printf("total : %d \n", total);

    rest2 = 10000 % data->interval;                   //나머지가 생기면 범위가 추가되어야함
    if (rest2 != 0) {
        data->int_size += 1;                          //범위의 개수에 +1 해줌
    }
    //printf("rest2 : %d, interval \n", rest2);
    result = (int *)malloc(sizeof(int) * data->int_size);

    pthread_mutex_init(&mutex, NULL);

    //입력받은 개수만큼 스레드 생성
    for(i = 0; i < data->tnum; i++) {
        check_tid = pthread_create(&tid[i], NULL, run_data, (void *)data);
        
        if(check_tid < 0) {
            perror("pthread_create()\n");
            exit(0);
        }
    }

    for ( i = 0; i < data->tnum; i++) {
        pthread_join(tid[i], NULL);
    }

    for (i = 0; i < data->int_size; i++) {
        printf("%d\n", result[i]);
    }

    pthread_mutex_destroy(&mutex);
    close(data->fd);
    free(tid);
    free(data);
    free(result);

    return 0;
}