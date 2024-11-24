// CPU 사용량
// 메모리 사용량
// 사용량이 큰 10개의 메모리 띄우기
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

float cpu_usage();
float memory_usage();
void top10_memory();

typedef struct
{
    int pid;
    int memory;
    char dname[256];
}processinfo;

processinfo top10[10];
int main()
{
    float cpu= cpu_usage();
    float memory= memory_usage();
    printf("cpu 사용량: %.2lf %%\n메모리 사용량: %.2lf %%\n",cpu,memory);
    top10_memory();
}
float cpu_usage()
{
    int user,nice,system,idle;
    static int prev_user,prev_nice, prev_system, prev_idle;
    //static은 자동으로 초기화가 된다. 

    float usage;
    //cpu 사용량 정보가 담겨 있는 파일을 연다
    FILE *fp;
    fp=fopen("/proc/stat","r");

    if(fp==NULL)
    {
        perror("file open error");
        exit(1);
    }

    fscanf(fp,"cpu %d %d %d %d",&user,&nice,&system,&idle);
    fclose(fp);

    int idle_difference;
    int total_difference;

    idle_difference = idle-prev_idle;
    total_difference = (user-prev_user) + (nice-prev_nice) + (system-prev_system) + (idle-prev_idle);

    usage = 100*(1.0-((float)idle_difference/(float)total_difference));

    prev_user = user;
    prev_nice = nice;
    prev_system = system;
    prev_idle = idle;
    //이렇게 갱신해주지 않으면 항상 첫 상태와의 차이만 계산된다.. 
    return usage;
}
float memory_usage()
{
    int memtotal,memfree;
    float memory;
    FILE *fp;
    fp=fopen("/proc/meminfo","r");

    if(fp==NULL)
    {
        perror("Cannot open file");
        exit(1);
    }
    fscanf(fp,"MemToal: %d kB\nMemFree: %dkB",&memtotal,&memfree);
    memory = 100*((float)memtotal/(float)memfree);

    return memory;
}
void top10_memory()
{
    DIR *dir = opendir("/proc");
    struct dirent *dirent_ptr;
    int count=0;
    processinfo processes[1024];

    if(dir==NULL)
    {
        perror("Cannot open directory");
        return;
    }

    //atoi(dirent_ptr->d_name)>0 숫자로 된 디렉토리를 말함 즉, pid로 검색할 수 있는 
    while((dirent_ptr=readdir(dir))!=NULL)
    {
        if(dirent_ptr->d_type==DT_DIR && atoi(dirent_ptr->d_name)>0)
        {
            //dirent_ptr에서 pid값을 받아오고
            // /proc/[pid]/status 에서 해당 pid의 메모리크기(Vimsize..) 갖고 온다
            char path[300];
            snprintf(path,sizeof(path),"/proc/%s/status",dirent_ptr->d_name);
            //path에 "/proc/[pid]/status"를 저장하는 거임 path의 크기만큼, 출력도 됨..

            FILE *fp = fopen(path,"r");
            if(fp==NULL)
            {
                perror("Cannot open file");
                continue;
            }
            char line[300];
            processinfo p;
            p.pid=atoi(dirent_ptr->d_name);
            p.memory=0;
            while(fgets(line,sizeof(line),fp))
            {
                if(strncmp(line,"Name:",5)==0)
                {
                    sscanf(line,"Name:%s",p.dname);
                }
                if(strncmp(line,"VmRSS:",6)==0)
                {
                    sscanf(line,"VmRSS: %d kB",&p.memory);
                }
            }
            fclose(fp);

            processes[count++]=p;
        }
    }
    closedir(dir);

    //정렬.. proceessinfo.memory가 큰 순서대로 정렬하면 된다.
    //내림차순하세요
    
    for(int i=0;i<count-1;i++)
    {
        for(int j=i+1;j<count;j++)
        {
            if(processes[j].memory>processes[i].memory)
            {
                processinfo temp = processes[i];
                processes[i]=processes[j];
                processes[j]=temp;
            }
        }
    }
    //상위 10개만 저장한다.. 
    for(int i=0;i<10;i++)
    {
        top10[i]=processes[i];
        printf("%d. %s(PID:%d) - %dkB\n",i+1,top10[i].dname,top10[i].pid,top10[i].memory);
    }

}